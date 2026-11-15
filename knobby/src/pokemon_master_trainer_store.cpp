#include "pokemon_master_trainer_store.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const uint32_t kMagic=0x31544D50UL; /* PMT1 */
typedef struct {
    uint32_t magic;
    uint32_t user_id;
    uint32_t generation;
    uint32_t unlock_game_id;
    uint8_t unlocked;
    uint8_t spent;
    uint16_t reserved;
    uint32_t checksum;
} mt_record_t;
static uint32_t checksum(const mt_record_t *r)
{
    const uint8_t *p=(const uint8_t *)r;
    uint32_t v=2166136261UL;
    for(size_t i=0;i<offsetof(mt_record_t,checksum);i++) {
        v^=p[i];v*=16777619UL;
    }
    return v;
}
static void path_for(uint32_t id,unsigned slot,char out[24])
{
    snprintf(out,24,"/pmt_%08lx%c.bin",(unsigned long)id,slot?'b':'a');
}
static bool valid_id(uint32_t id)
{
    return id!=0 && id<0xFFFFFFFCUL;
}
static bool read_record(const char *path,uint32_t user_id,mt_record_t *r)
{
    File f=SPIFFS.open(path,FILE_READ);
    if(!f)return false;
    const bool ok=f.size()==sizeof(*r) &&
                  f.read((uint8_t *)r,sizeof(*r))==(int)sizeof(*r);
    f.close();
    return ok && r->magic==kMagic && r->user_id==user_id &&
           r->generation>=1 && r->unlock_game_id!=0 &&
           r->unlocked==1 && r->spent<=1 && r->reserved==0 &&
           r->checksum==checksum(r);
}
/* Strictly validate BOTH existing slots before trusting either.
 * Previous-slot fallback is NOT safe for currency: corruption of the
 * newest slot could otherwise resurrect a spent Master Ball.
 */
static pk_mt_status_t inspect(uint32_t id,mt_record_t *out,
                              unsigned *latest_slot)
{
    char a[24],b[24];path_for(id,0,a);path_for(id,1,b);
    bool has_a=SPIFFS.exists(a),has_b=SPIFFS.exists(b);
    if(!has_a && !has_b)return PK_MT_MISSING;
    mt_record_t ra={},rb={};
    if(has_a && !read_record(a,id,&ra))return PK_MT_CORRUPT;
    if(has_b && !read_record(b,id,&rb))return PK_MT_CORRUPT;
    if(!has_a) return PK_MT_CORRUPT;
    if(!has_b){*out=ra;*latest_slot=0;return PK_MT_VALID;}
    if(ra.unlock_game_id!=rb.unlock_game_id ||
       ra.unlocked!=rb.unlocked ||
       (ra.spent && !rb.spent && ra.generation<rb.generation) ||
       (rb.spent && !ra.spent && rb.generation<ra.generation))
        return PK_MT_CORRUPT;
    if(ra.generation==rb.generation) return PK_MT_CORRUPT;
    const bool b_newer=(int32_t)(rb.generation-ra.generation)>0;
    const mt_record_t *newer=b_newer?&rb:&ra;
    const mt_record_t *older=b_newer?&ra:&rb;
    if((uint32_t)(newer->generation-older->generation)!=1U)
        return PK_MT_CORRUPT;
    *out=*newer;
    *latest_slot=b_newer?1U:0U;
    return PK_MT_VALID;
}
static bool write_verified(uint32_t id,unsigned slot,mt_record_t *r)
{
    char path[24];path_for(id,slot,path);
    r->checksum=checksum(r);
    File f=SPIFFS.open(path,FILE_WRITE);
    if(!f)return false;
    size_t count=f.write((const uint8_t *)r,sizeof(*r));
    f.flush();f.close();
    if(count!=sizeof(*r))return false;
    mt_record_t verified={};
    return read_record(path,id,&verified) &&
           memcmp(&verified,r,sizeof(*r))==0;
}
extern "C" pk_mt_status_t pk_mt_inspect(uint32_t id,pk_mt_state_t *out)
{
    if(!valid_id(id) || !SPIFFS.begin(false))return PK_MT_ERROR;
    mt_record_t r={};unsigned slot=0;
    const pk_mt_status_t status=inspect(id,&r,&slot);
    if(status==PK_MT_VALID && out) {
        out->unlocked=true;
        out->spent=r.spent!=0;
        out->unlock_game_id=r.unlock_game_id;
    }
    return status;
}
extern "C" bool pk_mt_unlock(uint32_t id,uint32_t game_id,
                              const pk_profile_t *before,
                              const pk_profile_t *after)
{
    if(!valid_id(id) || !game_id || !before || !after ||
       !pk_master_trainer_first_level_100(before,after) ||
       !SPIFFS.begin(false))return false;
    bool found_after=false,found_before=false;
    for(unsigned i=0;i<PK_RECENT_GAMES;i++) {
        if(after->recent_game_ids[i]==game_id)found_after=true;
        if(before->recent_game_ids[i]==game_id)found_before=true;
    }
    if(!found_after || found_before)return false;
    mt_record_t old={};unsigned slot=0;
    if(inspect(id,&old,&slot)!=PK_MT_MISSING)return false;
    mt_record_t first={};
    first.magic=kMagic;first.user_id=id;first.generation=1;
    first.unlock_game_id=game_id;first.unlocked=1;
    if(!write_verified(id,0,&first))return false;
    mt_record_t check={};unsigned verify_slot=0;
    return inspect(id,&check,&verify_slot)==PK_MT_VALID &&
           check.unlock_game_id==game_id && check.spent==0;
}
extern "C" bool pk_mt_mark_spent(uint32_t id)
{
    if(!valid_id(id) || !SPIFFS.begin(false))return false;
    mt_record_t current={};unsigned slot=0;
    if(inspect(id,&current,&slot)!=PK_MT_VALID ||
       !current.unlocked || current.spent ||
       current.generation==UINT32_MAX)return false;
    current.generation++;
    current.spent=1;
    if(!write_verified(id,1U-slot,&current))return false;
    mt_record_t check={};unsigned verify_slot=0;
    return inspect(id,&check,&verify_slot)==PK_MT_VALID &&
           check.spent==1 && verify_slot==1U-slot;
}
