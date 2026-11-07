#include "pokemon_starter_store.h"
#include "pokemon_player_identity.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static const uint32_t MAGIC=0x31525350U; /* PSR1 */
typedef struct {
    uint32_t magic;
    uint32_t id;
    uint32_t generation;
    pk_starter_state_t state;
    uint32_t checksum;
} Record;
static uint32_t hash(const Record *r){
    const uint8_t *p=(const uint8_t*)r;
    uint32_t h=2166136261U;
    for(size_t i=0;i<offsetof(Record,checksum);i++){h^=p[i];h*=16777619U;}
    return h;
}
static void path(uint32_t id,unsigned slot,char out[24]){
    snprintf(out,24,"/psr_%08lX%c.bin",(unsigned long)id,slot?'b':'a');
}
static bool read_slot(uint32_t id,unsigned slot,Record *r){
    char p[24];path(id,slot,p);
    File f=SPIFFS.open(p,FILE_READ);
    if(!f)return false;
    bool ok=f.size()==sizeof(*r) &&
            f.read((uint8_t*)r,sizeof(*r))==(int)sizeof(*r);
    f.close();
    return ok && r->magic==MAGIC && r->id==id &&
           r->checksum==hash(r) && pk_starter_state_valid(&r->state);
}
static pk_starter_store_status_t inspect(uint32_t id,Record *out,
                                        unsigned *slot){
    char a[24],b[24];path(id,0,a);path(id,1,b);
    bool ea=SPIFFS.exists(a),eb=SPIFFS.exists(b);
    if(!ea && !eb)return PK_STARTER_STORE_MISSING;
    Record ra={},rb={};
    /* In contrast to Pokémon team recovery, starter rolls must NEVER
     * fall back if one record was damaged: a lost reroll is worse than
     * fail-closed and manual recovery. */
    if((ea && !read_slot(id,0,&ra)) ||
       (eb && !read_slot(id,1,&rb)))return PK_STARTER_STORE_CORRUPT;
    if(ea && eb){
        if((int32_t)(rb.generation-ra.generation)>0){
            *out=rb;*slot=1U;
        }else{
            *out=ra;*slot=0U;
        }
    }else if(ea){*out=ra;*slot=0U;}
    else{*out=rb;*slot=1U;}
    return PK_STARTER_STORE_VALID;
}
static bool write(uint32_t id,unsigned slot,const Record *r){
    char p[24];path(id,slot,p);
    File f=SPIFFS.open(p,FILE_WRITE);
    if(!f)return false;
    size_t n=f.write((const uint8_t*)r,sizeof(*r));
    f.flush();f.close();
    if(n!=sizeof(*r))return false;
    Record check={};
    return read_slot(id,slot,&check) && check.generation==r->generation &&
           memcmp(&check.state,&r->state,sizeof(r->state))==0;
}
extern "C" pk_starter_store_status_t pk_starter_store_inspect(
    uint32_t id,pk_starter_state_t *out){
    if(!pk_player_id_valid(id) || !SPIFFS.begin(false))
        return PK_STARTER_STORE_ERROR;
    Record r={};unsigned slot=0;
    pk_starter_store_status_t s=inspect(id,&r,&slot);
    if(s==PK_STARTER_STORE_VALID && out)*out=r.state;
    return s;
}
extern "C" bool pk_starter_store_create(
    uint32_t id,const pk_starter_state_t *state){
    if(!pk_player_id_valid(id) || !pk_starter_state_valid(state) ||
       state->reveal_count!=1U || state->kept ||
       !SPIFFS.begin(false))return false;
    Record old={};unsigned slot=0;
    if(inspect(id,&old,&slot)!=PK_STARTER_STORE_MISSING)return false;
    Record r={};r.magic=MAGIC;r.id=id;r.generation=1U;
    r.state=*state;r.checksum=hash(&r);
    return write(id,0,&r);
}
extern "C" bool pk_starter_store_update(
    uint32_t id,const pk_starter_state_t *state){
    if(!pk_player_id_valid(id) || !pk_starter_state_valid(state) ||
       !SPIFFS.begin(false))return false;
    Record old={};unsigned slot=0;
    if(inspect(id,&old,&slot)!=PK_STARTER_STORE_VALID ||
       old.state.kept)return false;
    const pk_starter_state_t *prev=&old.state;
    bool is_keep=state->kept && state->reveal_count==prev->reveal_count &&
                 state->current.species==prev->current.species &&
                 state->current.shiny==prev->current.shiny &&
                 state->current.tier==prev->current.tier;
    bool is_reroll=!state->kept &&
                   state->reveal_count==(unsigned)prev->reveal_count+1U &&
                   prev->reveal_count<PK_STARTER_TOTAL_REVEALS &&
                   state->current.species!=prev->current.species;
    if(!is_keep && !is_reroll)return false;
    for(unsigned i=0;i<prev->reveal_count;i++)
        if(state->revealed_species[i]!=prev->revealed_species[i])
            return false;
    Record next={};next.magic=MAGIC;next.id=id;
    next.generation=old.generation+1U;next.state=*state;
    next.checksum=hash(&next);
    return write(id,1U-slot,&next);
}
