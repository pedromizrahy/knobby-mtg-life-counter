#include "pokemon_reward_coordinator.h"
#include <string.h>

static bool profile_has(const pk_profile_t *p,uint32_t session)
{
    if (!p || !session) return false;
    for (unsigned i=0;i<PK_RECENT_GAMES;i++)
        if (p->recent_game_ids[i]==session) return true;
    return false;
}
pk_tx_result_t pk_reward_transaction(const pk_reward_io_t *io,
         uint32_t user_id,uint32_t game_id,bool won,pk_reward_t *reward)
{
    if (!io || !user_id || !game_id || !reward ||
        !io->get_state || !io->record_state || !io->load_profile ||
        !io->save_profile || !io->find_pending) return PK_TX_ERROR;
    memset(reward,0,sizeof(*reward));
    pk_ledger_state_t state=io->get_state(io->ctx,user_id,game_id);
    if(state==PK_LEDGER_ERROR) return PK_TX_ERROR;
    if(state==PK_LEDGER_COMMITTED) return PK_TX_ALREADY;

    /* Before beginning any other game, resolve the oldest outstanding
     * transaction. This keeps recent_game_ids from silently rotating away. */
    uint32_t pending=0;
    if(!io->find_pending(io->ctx,user_id,&pending)) return PK_TX_ERROR;
    if(pending && pending!=game_id) return PK_TX_BLOCKED;

    pk_profile_t profile;
    if(!io->load_profile(io->ctx,user_id,&profile)) return PK_TX_NOT_READY;

    if(state==PK_LEDGER_PREPARED) {
        /* A crash after writing the profile but before COMMITTED: recover
         * without a second award. If no ID remains, it is AMBIGUOUS
         * whether the save happened. Do not guess or reapply. */
        if(!profile_has(&profile,game_id)) return PK_TX_BLOCKED;
        if(!io->record_state(io->ctx,user_id,game_id,PK_LEDGER_COMMITTED))
            return PK_TX_ERROR;
        return PK_TX_RECOVERED;
    }
    if(state!=PK_LEDGER_ABSENT || pending) return PK_TX_BLOCKED;
    /* If the journal disappeared but the profile still mentions the ID,
     * treat it as an inconsistency, not as permission to award twice. */
    if(profile_has(&profile,game_id)) return PK_TX_BLOCKED;
    pk_profile_t updated=profile;
    pk_reward_t calculated;
    if(!pk_award_game(&updated,game_id,won,&calculated))
        return PK_TX_NOT_READY;
    if(!io->record_state(io->ctx,user_id,game_id,PK_LEDGER_PREPARED))
        return PK_TX_ERROR;
    if(!io->save_profile(io->ctx,user_id,&updated))
        return PK_TX_ERROR;
    /* If commit fails, the next invocation will recover from PREPARED
     * using the game ID in the stored profile. */
    if(!io->record_state(io->ctx,user_id,game_id,PK_LEDGER_COMMITTED))
        return PK_TX_ERROR;
    *reward=calculated;
    return PK_TX_APPLIED;
}
