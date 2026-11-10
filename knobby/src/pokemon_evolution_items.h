#ifndef POKEMON_EVOLUTION_ITEMS_H
#define POKEMON_EVOLUTION_ITEMS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pure, platform-independent item inventory. NOT attached to Playgroup
 * account profiles until every Pokemon has a persisted stable instance ID.
 * In particular: never use party slot, species, nickname or user ID alone
 * as the Pokemon identity; evolution changes species, swapping changes slot. */
#define PK_ITEM_MAX_MON 6U
#define PK_ITEM_MILESTONES 3U
typedef enum {
    PK_ITEM_NONE=0,
    PK_ITEM_FIRE_STONE=1,
    PK_ITEM_WATER_STONE=2,
    PK_ITEM_THUNDER_STONE=3,
    PK_ITEM_LEAF_STONE=4,
    PK_ITEM_MOON_STONE=5,
    PK_ITEM_SUN_STONE=6,
    PK_ITEM_LINK_CABLE=7,
    PK_ITEM_METAL_COAT=8,
    PK_ITEM_KINGS_ROCK=9,
    PK_ITEM_DRAGON_SCALE=10,
    PK_ITEM_UP_GRADE=11,
    PK_ITEM_COUNT=12
} pk_item_type_t;
typedef enum {
    PK_ITEM_GROUP_NONE=0,
    PK_ITEM_GROUP_STONE=1,
    PK_ITEM_GROUP_SPECIAL=2,
    PK_ITEM_GROUP_ANY=3
} pk_item_group_t;
typedef struct {
    uint64_t instance_id; /* nonzero opaque immutable ID, supplied externally */
    uint8_t earned_mask;
    uint8_t claimed_mask;
} pk_item_mon_claims_t;
typedef struct {
    uint16_t count[PK_ITEM_COUNT];
    pk_item_mon_claims_t mons[PK_ITEM_MAX_MON];
} pk_item_ledger_t;
/* Levels 30, 50, 75. 30: any stone. 50: any special item.
 * 75: any item. Stable instance IDs must already have been assigned
 * and durably stored with the Pokemon before these functions are used. */
uint8_t pk_item_milestone_level(unsigned index);
pk_item_group_t pk_item_milestone_group(unsigned index);
const char *pk_item_name(pk_item_type_t item);
bool pk_item_allowed(pk_item_group_t group,pk_item_type_t item);
/* Atomic in-memory operations: failure changes NOTHING.
 * Caller must durably save ledger with generation/checksum before
 * presenting a reward as final to a player. No profile writes here. */
bool pk_item_register_mon(pk_item_ledger_t *ledger,uint64_t instance_id);
bool pk_item_level_sync(pk_item_ledger_t *ledger,uint64_t instance_id,
                        uint8_t level);
bool pk_item_claim(pk_item_ledger_t *ledger,uint64_t instance_id,
                   unsigned milestone,pk_item_type_t selection);
bool pk_item_consume(pk_item_ledger_t *ledger,pk_item_type_t item);
/* False when claim already used, not earned, or wrong item group. */
#ifdef __cplusplus
}
#endif
#endif
