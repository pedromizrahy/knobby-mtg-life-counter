#ifndef POKEMON_REWARD_STORAGE_H
#define POKEMON_REWARD_STORAGE_H
#include <stdint.h>
#include <stdbool.h>
#include "pokemon_reward_coordinator.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Explicit opt-in adapter for existing profiles only.
 * Does NOT create a Pokemon, hook postgame, grant rewards automatically
 * or change the Playgroup pending queue. The caller must have verified
 * confirmed/durable game outcome and player identity, and serialized
 * all requests for this user before calling it.
 */
pk_tx_result_t pk_reward_apply_existing(uint32_t user_id, uint32_t session_id,
                                        bool won, pk_reward_t *out);
#ifdef __cplusplus
}
#endif
#endif
