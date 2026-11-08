#include "pokemon_item_enrollment.h"
#include "pokemon_player_identity.h"
#include "pokemon_starter_store.h"
#include "pokemon_file_store.h"
#include "pokemon_profile_store.h"
#include <string.h>
/* Enroll only a single, explicitly confirmed initial Pokemon. No changes
 * to v1 profile; stable 64-bit identity lives in a separate sidecar.
 * The actual identity check against fresh Playgroup /me is the caller's
 * responsibility; this API cannot issue a network request by itself. */
extern "C" pk_item_enroll_result_t pk_item_enroll_confirmed_starter(
    uint32_t id,uint64_t unique_id,pk_item_ledger_t *out){
    if(!out || !pk_player_id_valid(id) || !unique_id)
        return PK_ITEM_ENROLL_BLOCKED;
    pk_starter_state_t starter={};
    if(pk_starter_store_inspect(id,&starter)!=PK_STARTER_STORE_VALID ||
       !starter.kept || !pk_starter_state_valid(&starter))
        return PK_ITEM_ENROLL_BLOCKED;
    pk_profile_t real={};
    if(pk_file_inspect(id,&real)!=PK_STORE_VALID ||
       pk_store_inspect(id,NULL)!=PK_STORE_MISSING ||
       real.version!=1U || real.team_count!=1U ||
       real.active_index!=0U ||
       real.team[0].species!=starter.current.species ||
       real.team[0].shiny!=starter.current.shiny)
        return PK_ITEM_ENROLL_BLOCKED;
    pk_item_ledger_t existing={};
    pk_item_store_status_t s=pk_item_store_inspect(id,&existing,NULL);
    if(s==PK_ITEM_STORE_VALID){
        if(!existing.mons[0].instance_id)return PK_ITEM_ENROLL_BLOCKED;
        *out=existing;
        return PK_ITEM_ENROLL_ALREADY;
    }
    if(s!=PK_ITEM_STORE_MISSING)return PK_ITEM_ENROLL_BLOCKED;
    pk_item_ledger_t fresh={};
    if(!pk_item_register_mon(&fresh,unique_id))return PK_ITEM_ENROLL_FAILED;
    /* Mirror earned milestones for the one verified existing starter,
     * but NEVER claim any item or change the profile at enrollment. */
    if(!pk_item_level_sync(&fresh,unique_id,real.team[0].level))
        return PK_ITEM_ENROLL_FAILED;
    if(!pk_item_store_create(id,&fresh))
        return PK_ITEM_ENROLL_FAILED;
    pk_item_ledger_t verify={};
    if(pk_item_store_inspect(id,&verify,NULL)!=PK_ITEM_STORE_VALID ||
       memcmp(&verify,&fresh,sizeof(fresh))!=0)
        return PK_ITEM_ENROLL_FAILED;
    *out=verify;
    return PK_ITEM_ENROLL_CREATED;
}
