#ifndef POKEMON_REWARD_MASTER_STORAGE_H
#define POKEMON_REWARD_MASTER_STORAGE_H
#include "pokemon_reward_master_coordinator.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Manual / opt-in entry point ONLY; NOT wired to postgame or UI.
 * Caller must verify Playgroup identity, durable game and outcome, serialize
 * per player, and validate power-loss recovery on hardware first.
 * Does not create a profile or migrate NVS/SPIFFS.
 */
pk_tx_result_t pk_reward_master_apply_existing(uint32_t user_id,
          uint32_t game_id,bool won,pk_reward_t *out);
#ifdef __cplusplus
}
#endif
#endif
