#ifndef POKEMON_REWARD_LEDGER_H
#define POKEMON_REWARD_LEDGER_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Explicit opt-in; never called from the Magic gameplay path.
 * A durable per-user journal of (session, state) transitions.
 * PREPARED is NOT an awarded game. A future coordinator must reconcile
 * PREPARED with the actual saved profile before adding COMMITTED.
 * CORRUPT/IO failures must fail CLOSED, never trigger a fresh reward.
 */
typedef enum {
    PK_LEDGER_ERROR = -1,
    PK_LEDGER_ABSENT = 0,
    PK_LEDGER_PREPARED = 1,
    PK_LEDGER_COMMITTED = 2
} pk_ledger_state_t;
pk_ledger_state_t pk_ledger_get(uint32_t user_id, uint32_t session_id);
bool pk_ledger_record(uint32_t user_id, uint32_t session_id,
                      pk_ledger_state_t new_state);
/* Returns false on invalid/corrupt storage; on success, pending_id is zero
 * when clean, or the first unresolved PREPARED session. */
bool pk_ledger_find_pending(uint32_t user_id, uint32_t *pending_id);
#ifdef __cplusplus
}
#endif
#endif
