#ifndef POKEMON_REWARD_COORDINATOR_H
#define POKEMON_REWARD_COORDINATOR_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_progress.h"
#include "pokemon_reward_ledger.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Platform-independent transaction coordinator. All persistence is injected,
 * so simulated power failures can be tested without writing to a real DIAL.
 * The caller MUST serialize requests per user (not safe to call concurrently).
 * No call sites are wired to gameplay in this phase.
 */
typedef struct {
    void *ctx;
    pk_ledger_state_t (*get_state)(void *,uint32_t,uint32_t);
    bool (*record_state)(void *,uint32_t,uint32_t,pk_ledger_state_t);
    bool (*load_profile)(void *,uint32_t,pk_profile_t *);
    bool (*save_profile)(void *,uint32_t,const pk_profile_t *);
    /* Returns false on I/O failure; true and pending_id=0 when clean.
       Must inspect the ENTIRE ledger and fail closed on corrupt records. */
    bool (*find_pending)(void *,uint32_t,uint32_t *pending_id);
} pk_reward_io_t;
typedef enum {
    PK_TX_ERROR=-1, PK_TX_NOT_READY=0, PK_TX_APPLIED=1,
    PK_TX_ALREADY=2, PK_TX_RECOVERED=3, PK_TX_BLOCKED=4
} pk_tx_result_t;
pk_tx_result_t pk_reward_transaction(const pk_reward_io_t *io,
        uint32_t user_id,uint32_t game_id,bool won,pk_reward_t *reward);
#ifdef __cplusplus
}
#endif
#endif
