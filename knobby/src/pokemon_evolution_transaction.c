#include "pokemon_evolution_transaction.h"
#include <string.h>
static bool ready(const pk_evolve_tx_io_t *io,uint32_t id){
    return io && id && io->intent_read && io->intent_create &&
           io->intent_complete && io->profile_read &&
           io->profile_save_if_unchanged;
}
static bool valid_intent(const pk_evolve_tx_intent_t *i,uint32_t id){
    if(!i || i->user_id!=id || !i->nonce || i->team_slot>=PK_TEAM_MAX ||
       i->from==0 || i->to==0 || i->from==i->to ||
       i->before.version!=1 || i->after.version!=1 ||
       i->before.team_count==0 || i->before.team_count>PK_TEAM_MAX ||
       i->team_slot>=i->before.team_count ||
       i->before.team[i->team_slot].species!=i->from ||
       i->after.team[i->team_slot].species!=i->to)return false;
    pk_profile_t recomputed={0};
    if(!pk_evo_prepare_level(&i->before,i->team_slot,i->to,&recomputed))
        return false;
    return memcmp(&recomputed,&i->after,sizeof(recomputed))==0;
}
static pk_evolve_tx_result_t finish(const pk_evolve_tx_io_t *io,
                                  const pk_evolve_tx_intent_t *intent){
    pk_profile_t live={0};
    if(!io->profile_read(io->ctx,intent->user_id,&live))
        return PK_EVOLVE_TX_RECOVERY_NEEDED;
    if(memcmp(&live,&intent->before,sizeof(live))==0){
        if(!io->profile_save_if_unchanged(io->ctx,intent->user_id,
                                          &intent->before,&intent->after))
            return PK_EVOLVE_TX_RECOVERY_NEEDED;
        if(!io->profile_read(io->ctx,intent->user_id,&live))
            return PK_EVOLVE_TX_RECOVERY_NEEDED;
    }
    /* A retry after a successful profile write must NOT reapply the
     * mutation or spend/regrant any item. */
    if(memcmp(&live,&intent->after,sizeof(live))!=0)
        return PK_EVOLVE_TX_RECOVERY_NEEDED;
    if(!io->intent_complete(io->ctx,intent))
        return PK_EVOLVE_TX_RECOVERY_NEEDED;
    return PK_EVOLVE_TX_DONE;
}
pk_evolve_tx_result_t pk_evolve_tx_begin(const pk_evolve_tx_io_t *io,
            uint32_t id,uint32_t nonce,uint8_t slot,uint16_t target){
    if(!ready(io,id) || !nonce || !target)return PK_EVOLVE_TX_BLOCKED;
    pk_evolve_tx_intent_t old={0};
    pk_evolve_intent_status_t status=io->intent_read(io->ctx,id,&old);
    if(status!=PK_EVOLVE_INTENT_MISSING)
        return PK_EVOLVE_TX_BLOCKED; /* no reentrancy, including completed */
    pk_profile_t before={0},after={0};
    if(!io->profile_read(io->ctx,id,&before) ||
       !pk_evo_prepare_level(&before,slot,target,&after))
        return PK_EVOLVE_TX_BLOCKED;
    pk_evolve_tx_intent_t intent={0};
    intent.user_id=id;intent.nonce=nonce;intent.team_slot=slot;
    intent.from=before.team[slot].species;intent.to=target;
    intent.before=before;intent.after=after;
    if(!valid_intent(&intent,id))return PK_EVOLVE_TX_BLOCKED;
    if(!io->intent_create(io->ctx,&intent))
        return PK_EVOLVE_TX_RECOVERY_NEEDED;
    return finish(io,&intent);
}
pk_evolve_tx_result_t pk_evolve_tx_recover(
    const pk_evolve_tx_io_t *io,uint32_t id){
    if(!ready(io,id))return PK_EVOLVE_TX_BLOCKED;
    pk_evolve_tx_intent_t intent={0};
    pk_evolve_intent_status_t status=io->intent_read(io->ctx,id,&intent);
    if(status==PK_EVOLVE_INTENT_COMMITTED &&
       valid_intent(&intent,id))return PK_EVOLVE_TX_ALREADY_DONE;
    if(status!=PK_EVOLVE_INTENT_PENDING || !valid_intent(&intent,id))
        return PK_EVOLVE_TX_BLOCKED;
    return finish(io,&intent);
}
