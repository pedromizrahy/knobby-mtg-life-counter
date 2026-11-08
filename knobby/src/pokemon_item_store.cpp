#include "pokemon_item_store.h"
#include "pokemon_player_identity.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const uint32_t MAGIC=0x31494B50U; /* PKI1 */
static const uint16_t VERSION=1U;
typedef struct {
    uint32_t magic;
    uint32_t user_id;
    uint32_t generation;
    uint16_t version;
    uint16_t reserved;
    pk_item_ledger_t ledger;
    uint32_t checksum;
} ItemRecord;
static void item_path(uint32_t id,unsigned slot,char out[24]){
    snprintf(out,24,"/pki_%08lX%c.bin",(unsigned long)id,slot?'b':'a');
}
static uint32_t digest(const ItemRecord *r){
    const uint8_t *b=(const uint8_t*)r;
    uint32_t h=2166136261U;
    for(size_t i=0;i<offsetof(ItemRecord,checksum);i++){
        h^=b[i];h*=16777619U;
    }
    return h;
}
static bool valid_ledger(const pk_item_ledger_t *ledger){
    if(!ledger || ledger->count[PK_ITEM_NONE]!=0U)return false;
    for(unsigned i=0;i<PK_ITEM_MAX_MON;i++){
        const pk_item_mon_claims_t *m=&ledger->mons[i];
        if(!m->instance_id){
            if(m->earned_mask || m->claimed_mask)return false;
            continue;
        }
        if((m->earned_mask|m->claimed_mask)>= (1U<<PK_ITEM_MILESTONES) ||
           (m->claimed_mask & ~m->earned_mask))return false;
        for(unsigned j=0;j<i;j++)
            if(ledger->mons[j].instance_id==m->instance_id)return false;
    }
    return true;
}
static bool slot_read(uint32_t id,unsigned which,ItemRecord *out){
    char name[24];item_path(id,which,name);
    File f=SPIFFS.open(name,FILE_READ);
    if(!f)return false;
    bool ok=f.size()==sizeof(*out) &&
          f.read((uint8_t*)out,sizeof(*out))==(int)sizeof(*out);
    f.close();
    return ok && out->magic==MAGIC && out->user_id==id &&
           out->version==VERSION && !out->reserved &&
           out->generation!=0U && out->checksum==digest(out) &&
           valid_ledger(&out->ledger);
}
static pk_item_store_status_t read_latest(uint32_t id,
                                            ItemRecord *out,unsigned *slot){
    char a[24],b[24];item_path(id,0,a);item_path(id,1,b);
    bool ea=SPIFFS.exists(a),eb=SPIFFS.exists(b);
    if(!ea && !eb)return PK_ITEM_STORE_MISSING;
    ItemRecord ra={},rb={};
    if((ea && !slot_read(id,0,&ra)) || (eb && !slot_read(id,1,&rb)))
        return PK_ITEM_STORE_CORRUPT;
    if(ea && eb){
        if((int32_t)(rb.generation-ra.generation)>0){
            *out=rb;*slot=1;
        }else{
            *out=ra;*slot=0;
        }
    }else if(ea){*out=ra;*slot=0;}
    else{*out=rb;*slot=1;}
    return PK_ITEM_STORE_VALID;
}
static bool write_record(uint32_t id,unsigned slot,
                         const ItemRecord *r){
    char name[24];item_path(id,slot,name);
    File f=SPIFFS.open(name,FILE_WRITE);
    if(!f)return false;
    size_t n=f.write((const uint8_t*)r,sizeof(*r));
    f.flush();f.close();
    if(n!=sizeof(*r))return false;
    ItemRecord check={};
    return slot_read(id,slot,&check) && check.generation==r->generation &&
           memcmp(&check.ledger,&r->ledger,sizeof(r->ledger))==0;
}
extern "C" pk_item_store_status_t pk_item_store_inspect(
      uint32_t id,pk_item_ledger_t *out,uint32_t *generation){
    if(!pk_player_id_valid(id) || !SPIFFS.begin(false))
        return PK_ITEM_STORE_ERROR;
    ItemRecord current={};unsigned slot=0;
    pk_item_store_status_t status=read_latest(id,&current,&slot);
    if(status==PK_ITEM_STORE_VALID){
        if(out)*out=current.ledger;
        if(generation)*generation=current.generation;
    }
    return status;
}
extern "C" bool pk_item_store_create(uint32_t id,
                          const pk_item_ledger_t *ledger){
    if(!pk_player_id_valid(id) || !valid_ledger(ledger) ||
       !SPIFFS.begin(false))return false;
    ItemRecord found={};unsigned slot=0;
    if(read_latest(id,&found,&slot)!=PK_ITEM_STORE_MISSING)return false;
    ItemRecord next={};
    next.magic=MAGIC;next.user_id=id;next.generation=1U;
    next.version=VERSION;next.ledger=*ledger;
    next.checksum=digest(&next);
    return write_record(id,0,&next);
}
extern "C" bool pk_item_store_update(uint32_t id,uint32_t expected,
                          const pk_item_ledger_t *ledger){
    if(!pk_player_id_valid(id) || !valid_ledger(ledger) ||
       !expected || expected==UINT32_MAX || !SPIFFS.begin(false))
        return false;
    ItemRecord current={};unsigned slot=0;
    if(read_latest(id,&current,&slot)!=PK_ITEM_STORE_VALID ||
       current.generation!=expected)return false;
    /* Immutable existing Pokemon instance IDs; new nonzero slots are
     * allowed for later catches but existing IDs cannot be swapped or
     * replaced without a dedicated, deliberate migration. */
    for(unsigned i=0;i<PK_ITEM_MAX_MON;i++){
        const pk_item_mon_claims_t *old=&current.ledger.mons[i];
        const pk_item_mon_claims_t *mon=&ledger->mons[i];
        if(old->instance_id && old->instance_id!=mon->instance_id)return false;
        if((old->claimed_mask&~mon->claimed_mask) ||
           (old->earned_mask&~mon->earned_mask))return false;
    }
    ItemRecord next={};
    next.magic=MAGIC;next.user_id=id;next.version=VERSION;
    next.generation=expected+1U;next.ledger=*ledger;
    next.checksum=digest(&next);
    return write_record(id,1U-slot,&next);
}
