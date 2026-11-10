#ifndef POKEMON_ITEM_STORE_H
#define POKEMON_ITEM_STORE_H
#include <stdint.h>
#include <stdbool.h>
#include "pokemon_evolution_items.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Separate sidecar ledger: DOES NOT change version-1 Pokemon profile binary.
 * The account ID is resolved by the caller from fresh Playgroup /me.
 * No implicit create, migration, item award, or profile mutation. */
typedef enum {
    PK_ITEM_STORE_ERROR=-2,PK_ITEM_STORE_CORRUPT=-1,
    PK_ITEM_STORE_MISSING=0,PK_ITEM_STORE_VALID=1
} pk_item_store_status_t;
pk_item_store_status_t pk_item_store_inspect(uint32_t user_id,
                  pk_item_ledger_t *out,uint32_t *generation);
bool pk_item_store_create(uint32_t user_id,
                          const pk_item_ledger_t *ledger);
/* Optimistic version check stops replay of a stale claim.
 * Persist the WHOLE ledger (IDs + claim bits + item counters) in one slot.
 * Do not treat corrupt as empty or fall back to an older slot. */
bool pk_item_store_update(uint32_t user_id,uint32_t expected_generation,
                          const pk_item_ledger_t *ledger);
#ifdef __cplusplus
}
#endif
#endif
