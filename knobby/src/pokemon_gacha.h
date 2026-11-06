#ifndef POKEMON_GACHA_H
#define POKEMON_GACHA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pokemon_progress.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Pure gacha preview. No profile, ball or filesystem mutation.
 * Every species in the catalog must have a usable, installed sprite.
 * Callers must securely persist a single reveal + debit before animation.
 * No gacha is connected to gameplay/UI in this phase.
 */
typedef enum {
    PK_TIER_COMMON=0,PK_TIER_RARE=1,
    PK_TIER_EPIC=2,PK_TIER_LEGENDARY=3,
    PK_TIER_COUNT=4
} pk_gacha_tier_t;
typedef struct {
    uint16_t species;
    uint8_t tier;
    uint8_t sprite_ready;
} pk_gacha_entry_t;
typedef struct {
    uint16_t species;
    uint8_t tier;
    uint8_t shiny;
} pk_gacha_result_t;
typedef bool (*pk_gacha_rng_t)(void *ctx,uint32_t *out);
/* Default shiny chance 1/4096, configurable later. */
#define PK_GACHA_SHINY_DENOMINATOR 4096U
/* Returns true only if the full draw succeeds. Missing tier/assets
 * or RNG failure leaves output unchanged. Does not consume a Pokeball.
 */
bool pk_gacha_preview(uint8_t ball,
                      const pk_gacha_entry_t *catalog,size_t count,
                      pk_gacha_rng_t rng,void *ctx,
                      pk_gacha_result_t *out);
/* Pure documented weights in basis points, sum 10,000 for each ball. */
uint16_t pk_gacha_weight(uint8_t ball,uint8_t tier);
#ifdef __cplusplus
}
#endif
#endif
