#ifndef POKEMON_ITEM_REWARD_SERVICE_H
#define POKEMON_ITEM_REWARD_SERVICE_H
#include <stdint.h>
#include "pokemon_item_store.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Safe staged adapter for CONFIRMED starter only.
 * The application controller must fresh-verify /me == user_id and
 * reject active games. No automatic boot/game/serial invocation.
 * No new profile or sidecar is created here. */
typedef enum {
    PK_ITEM_REWARD_FAILED=0,PK_ITEM_REWARD_SYNCED=1,
    PK_ITEM_REWARD_UNCHANGED=2,PK_ITEM_REWARD_CLAIMED=3,
    PK_ITEM_REWARD_BLOCKED=4
} pk_item_reward_result_t;
/* Synchronize earned milestone bits from the persisted Pokémon level.
 * Only the one verified KEEP starter; does NOT award an item. */
pk_item_reward_result_t pk_item_reward_sync_starter(uint32_t user_id,
                                                   pk_item_ledger_t *out);
/* Explicit choice; entire claim+credit is one versioned sidecar write.
 * A retry cannot credit a second item for the same milestone. */
pk_item_reward_result_t pk_item_reward_claim_starter(
    uint32_t user_id,unsigned milestone,pk_item_type_t selected,
    pk_item_ledger_t *out);
#ifdef __cplusplus
}
#endif
#endif
