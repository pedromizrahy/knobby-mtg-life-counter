#include "pokemon_reward_master_coordinator.h"
#include "pokemon_achievements.h"
#include <string.h>

static bool profile_has(const pk_profile_t *p,uint32_t session)
{
    if(!p || !session)return false;
    for(unsigned i=0;i<PK_RECENT_GAMES;i++)
        if(p->recent_game_ids[i]==session)return true;
    return false;
}

pk_tx_result_t pk_reward_master_transaction(const pk_reward_master_io_t *io,
             uint32_t user_id,uint32_t game_id,bool won,pk_reward_t *reward)
{
    if(!io || !user_id || !game_id || !reward ||
       !io->games.get_state || !io->games.record_state ||
       !io->games.load_profile || !io->games.save_profile ||
       !io->games.find_pending || !io->intent_status ||
       !io->intent_prepare || !io->intent_finalize)
        return PK_TX_ERROR;
    memset(reward,0,sizeof(*reward));
    const pk_reward_io_t *g=&io->games;
    void *ctx=g->ctx;
    uint32_t intent_game=0;
    pk_mt_intent_status_t ist=io->intent_status(ctx,user_id,&intent_game);
    if(ist!=PK_MT_INTENT_MISSING && ist!=PK_MT_INTENT_VALID)
        return PK_TX_BLOCKED;
    if(ist==PK_MT_INTENT_VALID) {
        if(!intent_game)return PK_TX_BLOCKED;
        /* Never let another game overwrite the exact profile snapshot
         * needed to finalize an outstanding Master Trainer award. */
        if(g->get_state(ctx,user_id,intent_game)==PK_LEDGER_COMMITTED) {
            if(!io->intent_finalize(ctx,user_id,intent_game))
                return PK_TX_BLOCKED;
        } else if(intent_game!=game_id) {
            return PK_TX_BLOCKED;
        }
    }
    pk_ledger_state_t state=g->get_state(ctx,user_id,game_id);
    if(state==PK_LEDGER_ERROR)return PK_TX_ERROR;
    if(state==PK_LEDGER_COMMITTED) {
        if(ist==PK_MT_INTENT_VALID && intent_game==game_id &&
           !io->intent_finalize(ctx,user_id,game_id))
            return PK_TX_BLOCKED;
        return PK_TX_ALREADY;
    }
    uint32_t pending=0;
    if(!g->find_pending(ctx,user_id,&pending))return PK_TX_ERROR;
    if(pending && pending!=game_id)return PK_TX_BLOCKED;
    pk_profile_t profile;
    if(!g->load_profile(ctx,user_id,&profile))return PK_TX_NOT_READY;
    if(state==PK_LEDGER_PREPARED) {
        /* A missing game ID is ambiguous, never reapply. An intent
         * remains recoverable and is finalized after COMMITTED. */
        if(!profile_has(&profile,game_id))return PK_TX_BLOCKED;
        if(!g->record_state(ctx,user_id,game_id,PK_LEDGER_COMMITTED))
            return PK_TX_ERROR;
        if(ist==PK_MT_INTENT_VALID && intent_game==game_id &&
           !io->intent_finalize(ctx,user_id,game_id))
            return PK_TX_BLOCKED;
        return PK_TX_RECOVERED;
    }
    if(state!=PK_LEDGER_ABSENT || pending || profile_has(&profile,game_id))
        return PK_TX_BLOCKED;
    /* Existing uncommitted intent means a prior attempt may have
     * stopped before PREPARED. Do not guess whether to restart it. */
    if(ist==PK_MT_INTENT_VALID)return PK_TX_BLOCKED;

    pk_profile_t updated=profile;
    pk_reward_t calculated;
    if(!pk_award_game(&updated,game_id,won,&calculated))
        return PK_TX_NOT_READY;
    bool milestone=pk_master_trainer_first_level_100(&profile,&updated);
    if(milestone && !io->intent_prepare(ctx,user_id,game_id,&profile,&updated))
        return PK_TX_ERROR;
    if(!g->record_state(ctx,user_id,game_id,PK_LEDGER_PREPARED))
        return PK_TX_ERROR;
    if(!g->save_profile(ctx,user_id,&updated))
        return PK_TX_ERROR;
    if(!g->record_state(ctx,user_id,game_id,PK_LEDGER_COMMITTED))
        return PK_TX_ERROR;
    if(milestone && !io->intent_finalize(ctx,user_id,game_id))
        return PK_TX_BLOCKED;
    *reward=calculated;
    return PK_TX_APPLIED;
}
