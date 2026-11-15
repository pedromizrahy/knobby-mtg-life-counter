#include "pokemon_evolution_items.h"
#include <limits.h>
static const uint8_t kLevel[PK_ITEM_MILESTONES]={30U,50U,75U};
static const pk_item_group_t kGroup[PK_ITEM_MILESTONES]={
    PK_ITEM_GROUP_STONE,PK_ITEM_GROUP_SPECIAL,PK_ITEM_GROUP_ANY
};
uint8_t pk_item_milestone_level(unsigned index){
    return index<PK_ITEM_MILESTONES?kLevel[index]:0U;
}
pk_item_group_t pk_item_milestone_group(unsigned index){
    return index<PK_ITEM_MILESTONES?kGroup[index]:PK_ITEM_GROUP_NONE;
}
const char *pk_item_name(pk_item_type_t item){
    static const char *const names[PK_ITEM_COUNT]={
        "NONE","FIRE STONE","WATER STONE","THUNDER STONE",
        "LEAF STONE","MOON STONE","SUN STONE","LINK CABLE",
        "METAL COAT","KINGS ROCK","DRAGON SCALE","UP GRADE"
    };
    return item>PK_ITEM_NONE && item<PK_ITEM_COUNT?names[item]:0;
}
bool pk_item_allowed(pk_item_group_t group,pk_item_type_t item){
    if(item<=PK_ITEM_NONE || item>=PK_ITEM_COUNT)return false;
    if(group==PK_ITEM_GROUP_ANY)return true;
    if(group==PK_ITEM_GROUP_STONE)return item<=PK_ITEM_SUN_STONE;
    if(group==PK_ITEM_GROUP_SPECIAL)return item>=PK_ITEM_LINK_CABLE;
    return false;
}
static int index_of(const pk_item_ledger_t *ledger,uint64_t id){
    if(!ledger || !id)return -1;
    for(unsigned i=0;i<PK_ITEM_MAX_MON;i++)
        if(ledger->mons[i].instance_id==id)return (int)i;
    return -1;
}
bool pk_item_register_mon(pk_item_ledger_t *ledger,uint64_t id){
    if(!ledger || !id || index_of(ledger,id)>=0)return false;
    for(unsigned i=0;i<PK_ITEM_MAX_MON;i++)
        if(!ledger->mons[i].instance_id){
            ledger->mons[i].instance_id=id;
            ledger->mons[i].earned_mask=0;
            ledger->mons[i].claimed_mask=0;
            return true;
        }
    return false;
}
bool pk_item_level_sync(pk_item_ledger_t *ledger,uint64_t id,uint8_t level){
    int i=index_of(ledger,id);
    if(i<0 || level<1U || level>100U)return false;
    uint8_t earned=ledger->mons[i].earned_mask;
    for(unsigned j=0;j<PK_ITEM_MILESTONES;j++)
        if(level>=kLevel[j])earned|=(uint8_t)(1U<<j);
    ledger->mons[i].earned_mask=earned;
    return true;
}
bool pk_item_claim(pk_item_ledger_t *ledger,uint64_t id,
                   unsigned milestone,pk_item_type_t item){
    int i=index_of(ledger,id);
    if(i<0 || milestone>=PK_ITEM_MILESTONES ||
       !pk_item_allowed(kGroup[milestone],item))return false;
    uint8_t bit=(uint8_t)(1U<<milestone);
    pk_item_mon_claims_t *mon=&ledger->mons[i];
    if(!(mon->earned_mask&bit) || (mon->claimed_mask&bit) ||
       ledger->count[item]==UINT16_MAX)return false;
    /* Mark the once-per-Pokemon claim in the same in-memory transaction
     * as the inventory credit; caller must persist the entire ledger. */
    mon->claimed_mask|=bit;
    ledger->count[item]++;
    return true;
}
bool pk_item_consume(pk_item_ledger_t *ledger,pk_item_type_t item){
    if(!ledger || item<=PK_ITEM_NONE || item>=PK_ITEM_COUNT ||
       !ledger->count[item])return false;
    ledger->count[item]--;
    return true;
}
