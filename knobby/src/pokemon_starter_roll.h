#ifndef POKEMON_STARTER_ROLL_H
#define POKEMON_STARTER_ROLL_H
#include "pokemon_gacha.h"
#ifdef __cplusplus
extern "C" {
#endif
/* KEEP or REROLL: one visible result at a time, five rerolls maximum.
 * A REROLL permanently discards the prior result: no gallery/past choice.
 * All attempts use ordinary Pokeball rarity, independent of reroll count.
 * Real UI must persist state before showing each result; production
 * activation stays disabled until crash-safe persistence is integrated. */
#define PK_STARTER_REROLLS 5U
#define PK_STARTER_TOTAL_REVEALS (PK_STARTER_REROLLS+1U)
typedef struct {
    pk_gacha_result_t current;
    uint16_t revealed_species[PK_STARTER_TOTAL_REVEALS];
    uint8_t reveal_count;
    bool kept;
} pk_starter_state_t;
bool pk_starter_state_valid(const pk_starter_state_t *state);
bool pk_starter_begin(pk_starter_state_t *state,
                      const pk_gacha_entry_t *catalog,size_t count,
                      pk_gacha_rng_t rng,void *ctx);
bool pk_starter_reroll(pk_starter_state_t *state,
                      const pk_gacha_entry_t *catalog,size_t count,
                      pk_gacha_rng_t rng,void *ctx);
bool pk_starter_keep(pk_starter_state_t *state,
                     pk_gacha_result_t *chosen);
#ifdef __cplusplus
}
#endif
#endif
