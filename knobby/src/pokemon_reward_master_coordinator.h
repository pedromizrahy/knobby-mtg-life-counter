#ifndef POKEMON_REWARD_MASTER_COORDINATOR_H
#define POKEMON_REWARD_MASTER_COORDINATOR_H
#include "pokemon_reward_coordinator.h"
#include "pokemon_master_trainer_intent.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Opt-in coordinator integrating XP and Master Trainer intent ordering.
 * No calls from gameplay or existing reward adapter.
 * All per-user writes MUST be serialized; confirmed game results required.
 * The callbacks are intentionally injectable for failure simulations.
 */
typedef struct {
    pk_reward_io_t games;
    pk_mt_intent_status_t (*intent_status)(void *,uint32_t,uint32_t *);
    bool (*intent_prepare)(void *,uint32_t,uint32_t,
                           const pk_profile_t *,const pk_profile_t *);
    bool (*intent_finalize)(void *,uint32_t,uint32_t);
} pk_reward_master_io_t;
pk_tx_result_t pk_reward_master_transaction(const pk_reward_master_io_t *io,
             uint32_t user_id,uint32_t game_id,bool won,pk_reward_t *reward);
#ifdef __cplusplus
}
#endif
#endif
