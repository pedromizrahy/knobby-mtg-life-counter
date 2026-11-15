#include "pokemon_item_reward_service.h"
#include "pokemon_player_identity.h"
#include "pokemon_starter_store.h"
#include "pokemon_file_store.h"
#include "pokemon_profile_store.h"
#include <string.h>
/* Sidecar earned flags are monotonic and credited item claims are
 * committed atomically with claim bits. The canonical profile itself
 * is never written; existing v1 byte layout stays unchanged. */
static bool starter_matches(uint32_t id,pk_item_ledger_t *ledger,
                            uint8_t *level){
    if(!pk_player_id_valid(id) || !ledger || !level)return false;
    pk_starter_state_t starter={};
    if(pk_starter_store_inspect(id,&starter)!=PK_STARTER_STORE_VALID ||
       !starter.kept || !pk_starter_state_valid(&starter))return false;
    pk_profile_t profile={};
    if(pk_file_inspect(id,&profile)!=PK_STORE_VALID ||
       pk_store_inspect(id,NULL)!=PK_STORE_MISSING ||
       profile.version!=1U || profile.team_count!=1U ||
       profile.active_index!=0U ||
       profile.team[0].species!=starter.current.species ||
       profile.team[0].shiny!=starter.current.shiny ||
       profile.team[0].level<1U || profile.team[0].level>100U)
       return false;
    /* Only the original starter: other catches and evolved forms must
     * obtain their own stable instance binding before use. */
    if(!ledger->mons[0].instance_id)return false;
    for(unsigned i=1;i<PK_ITEM_MAX_MON;i++)
        if(ledger->mons[i].instance_id)return false;
    *level=profile.team[0].level;
    return true;
}
static pk_item_reward_result_t commit(uint32_t id,uint32_t generation,
              const pk_item_ledger_t *next,pk_item_ledger_t *out,
              pk_item_reward_result_t result){
    if(!pk_item_store_update(id,generation,next))
        return PK_ITEM_REWARD_FAILED;
    pk_item_ledger_t check={};
    uint32_t newgen=0;
    if(pk_item_store_inspect(id,&check,&newgen)!=PK_ITEM_STORE_VALID ||
       newgen!=generation+1U ||
       memcmp(&check,next,sizeof(check))!=0)
        return PK_ITEM_REWARD_FAILED;
    *out=check;
    return result;
}
extern "C" pk_item_reward_result_t pk_item_reward_sync_starter(
                         uint32_t id,pk_item_ledger_t *out){
    if(!out)return PK_ITEM_REWARD_BLOCKED;
    pk_item_ledger_t ledger={};
    uint32_t gen=0;
    if(pk_item_store_inspect(id,&ledger,&gen)!=PK_ITEM_STORE_VALID)
        return PK_ITEM_REWARD_BLOCKED;
    uint8_t level=0;
    if(!starter_matches(id,&ledger,&level))
        return PK_ITEM_REWARD_BLOCKED;
    pk_item_ledger_t next=ledger;
    if(!pk_item_level_sync(&next,ledger.mons[0].instance_id,level))
        return PK_ITEM_REWARD_BLOCKED;
    if(memcmp(&ledger,&next,sizeof(next))==0){
        *out=ledger;
        return PK_ITEM_REWARD_UNCHANGED;
    }
    return commit(id,gen,&next,out,PK_ITEM_REWARD_SYNCED);
}
extern "C" pk_item_reward_result_t pk_item_reward_claim_starter(
                    uint32_t id,unsigned milestone,pk_item_type_t selected,
                    pk_item_ledger_t *out){
    if(!out)return PK_ITEM_REWARD_BLOCKED;
    pk_item_ledger_t ledger={};
    uint32_t gen=0;
    if(pk_item_store_inspect(id,&ledger,&gen)!=PK_ITEM_STORE_VALID)
        return PK_ITEM_REWARD_BLOCKED;
    uint8_t level=0;
    if(!starter_matches(id,&ledger,&level) ||
       milestone>=PK_ITEM_MILESTONES ||
       level<pk_item_milestone_level(milestone))
        return PK_ITEM_REWARD_BLOCKED;
    pk_item_ledger_t next=ledger;
    /* Sync and claim together; interrupted save cannot grant an item
     * while leaving the corresponding once-only claim marker unset. */
    if(!pk_item_level_sync(&next,ledger.mons[0].instance_id,level) ||
       !pk_item_claim(&next,ledger.mons[0].instance_id,milestone,selected))
        return PK_ITEM_REWARD_BLOCKED;
    return commit(id,gen,&next,out,PK_ITEM_REWARD_CLAIMED);
}
