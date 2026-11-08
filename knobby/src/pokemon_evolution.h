#ifndef POKEMON_EVOLUTION_H
#define POKEMON_EVOLUTION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pokemon_progress.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Static offline policy sourced from PokeAPI species/evolution CSV.
 * Strict early-form gacha: no Pokemon with a pre-evolution anywhere in
 * National Dex, even if that baby species lacks an installed SD sprite.
 * The initial 251-species era is supported, no automatic profile mutation. */
typedef enum {PK_EVO_LEVEL=1, PK_EVO_STONE=2, PK_EVO_TRADE=3,
              PK_EVO_FRIENDSHIP=4, PK_EVO_SPECIAL=5} pk_evo_kind_t;
typedef struct {uint16_t from,to;uint8_t kind,level;uint16_t item_id;} pk_evo_rule_t;
/* Strict true only if the Pokemon has NO pre-evolution in the full dex.
 * The caller must separately confirm the sprite is available on the SD. */
bool pk_evo_is_base(uint16_t species);
/* Returns total matching option count; only copies if capacity is enough.
 * No partial writes. Evolution branches (like Eevee) are explicit. */
size_t pk_evo_options(uint16_t from,pk_evo_rule_t *out,size_t capacity);
/* Pure, explicit level-up eligibility. Never auto-evolve or save. */
bool pk_evo_level_eligible(uint16_t from,uint16_t to,uint8_t level);
/* Applies confirmed level evolution to a COPY of a profile.
 * Caller is responsible for durable verified save after user consent.
 * Preserves XP, level, shiny and all other Pokemon in team. */
bool pk_evo_prepare_level(const pk_profile_t *original,
                          uint8_t team_slot,uint16_t to,
                          pk_profile_t *modified);
#ifdef __cplusplus
}
#endif
#endif
