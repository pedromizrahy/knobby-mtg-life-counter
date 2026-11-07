#ifndef POKEMON_STARTER_ROLL_H
#define POKEMON_STARTER_ROLL_H
#include "pokemon_gacha.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One opening + five Pokeball-tier rerolls; player may choose any reveal.
 * Pure sampler: no account creation, SD, SPIFFS, NVS, or reward writes.
 * Real UI must durably persist each reveal before displaying it.
 * Without such persistence, production activation MUST stay disabled. */
#define PK_STARTER_OPTIONS 6U
#define PK_STARTER_REROLLS 5U
bool pk_starter_roll_six(const pk_gacha_entry_t *catalog,size_t count,
                         pk_gacha_rng_t rng,void *ctx,
                         pk_gacha_result_t out[PK_STARTER_OPTIONS]);
bool pk_starter_choice_valid(const pk_gacha_result_t choices[PK_STARTER_OPTIONS],
                             unsigned count,unsigned chosen);
#ifdef __cplusplus
}
#endif
#endif
