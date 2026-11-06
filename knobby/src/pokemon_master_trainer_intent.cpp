#include "pokemon_master_trainer_intent.h"
#include "pokemon_achievements.h"
#include "pokemon_master_trainer_store.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t magic;
    uint32_t user_id;
    uint32_t game_id;
    pk_profile_t before;
    pk_profile_t after;
    uint32_t checksum;
} intent_record_t;
static const uint32_t kMagic=0x31494D50UL; /* PMI1 */
static bool valid_id(uint32_t id){return id!=0 && id<0xFFFFFFFCUL;}
static void path_for(uint32_t id,char path[24])
{
    snprintf(path,24,"/pmi_%08lx.bin",(unsigned long)id);
}
static uint32_t checksum(const intent_record_t *r)
{
    uint32_t v=2166136261UL;
    const uint8_t *data=(const uint8_t *)r;
    for(size_t i=0;i<offsetof(intent_record_t,checksum);i++){
        v^=data[i];v*=16777619UL;
    }
    return v;
}
static pk_mt_intent_status_t read_intent(uint32_t id,intent_record_t *r)
{
    char path[24];path_for(id,path);
    if(!SPIFFS.exists(path))return PK_MT_INTENT_MISSING;
    File f=SPIFFS.open(path,FILE_READ);
    if(!f)return PK_MT_INTENT_ERROR;
    bool ok=f.size()==sizeof(*r) &&
        f.read((uint8_t *)r,sizeof(*r))==(int)sizeof(*r);
    f.close();
    if(!ok || r->magic!=kMagic || r->user_id!=id ||
       r->game_id==0 || r->checksum!=checksum(r) ||
       !pk_master_trainer_first_level_100(&r->before,&r->after))
        return PK_MT_INTENT_CONFLICT;
    unsigned b=0,a=0;
    for(unsigned i=0;i<PK_RECENT_GAMES;i++){
        if(r->before.recent_game_ids[i]==r->game_id)b++;
        if(r->after.recent_game_ids[i]==r->game_id)a++;
    }
    return a==1 && b==0 ? PK_MT_INTENT_VALID : PK_MT_INTENT_CONFLICT;
}
extern "C" pk_mt_intent_status_t
pk_mt_intent_status(uint32_t id,uint32_t *game_id)
{
    if(!valid_id(id) || !SPIFFS.begin(false))return PK_MT_INTENT_ERROR;
    intent_record_t r={};
    pk_mt_intent_status_t status=read_intent(id,&r);
    if(status==PK_MT_INTENT_VALID && game_id)*game_id=r.game_id;
    return status;
}
extern "C" bool pk_mt_intent_prepare(uint32_t id,uint32_t game_id,
                          const pk_profile_t *before,const pk_profile_t *after)
{
    if(!valid_id(id) || !game_id || !before || !after ||
       !pk_master_trainer_first_level_100(before,after) ||
       !SPIFFS.begin(false)) return false;
    intent_record_t existing={};
    if(read_intent(id,&existing)!=PK_MT_INTENT_MISSING)return false;
    /* Never start a second unlock while another wallet exists or is corrupt. */
    if(pk_mt_inspect(id,NULL)!=PK_MT_MISSING)return false;
    intent_record_t r={};
    r.magic=kMagic;r.user_id=id;r.game_id=game_id;
    r.before=*before;r.after=*after;
    r.checksum=checksum(&r);
    unsigned count_before=0,count_after=0;
    for(unsigned i=0;i<PK_RECENT_GAMES;i++){
        if(before->recent_game_ids[i]==game_id)count_before++;
        if(after->recent_game_ids[i]==game_id)count_after++;
    }
    if(count_before || count_after!=1)return false;
    char path[24];path_for(id,path);
    File f=SPIFFS.open(path,FILE_WRITE);
    if(!f)return false;
    size_t written=f.write((const uint8_t *)&r,sizeof(r));
    f.flush();f.close();
    if(written!=sizeof(r))return false;
    intent_record_t verified={};
    return read_intent(id,&verified)==PK_MT_INTENT_VALID &&
           memcmp(&verified,&r,sizeof(r))==0;
}
extern "C" bool pk_mt_intent_finalize(uint32_t id,uint32_t game_id,
                                      const pk_mt_recovery_io_t *io)
{
    if(!valid_id(id) || !game_id || !io || !io->get_ledger ||
       !io->load_profile || !SPIFFS.begin(false)) return false;
    intent_record_t r={};
    if(read_intent(id,&r)!=PK_MT_INTENT_VALID || r.game_id!=game_id)
        return false;
    if(io->get_ledger(io->ctx,id,game_id)!=PK_LEDGER_COMMITTED)
        return false;
    pk_profile_t persisted={};
    if(!io->load_profile(io->ctx,id,&persisted)) return false;
    /* Do not trust just the level or game ID: any divergent profile
     * requires reconciliation, never a guessed prize. */
    if(memcmp(&persisted,&r.after,sizeof(persisted))!=0)return false;
    pk_mt_state_t wallet={};
    const pk_mt_status_t status=pk_mt_inspect(id,&wallet);
    if(status==PK_MT_VALID)
        return wallet.unlocked && wallet.unlock_game_id==game_id;
    if(status!=PK_MT_MISSING)return false;
    return pk_mt_unlock(id,game_id,&r.before,&r.after);
}
