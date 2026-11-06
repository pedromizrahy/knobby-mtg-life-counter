#ifndef POKEMON_LAB_REVEAL_H
#define POKEMON_LAB_REVEAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pokemon_gacha.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Names for four synthetic diagnostic fixtures ONLY.
 * No real Pokedex ownership, account or asset lookup. */
const char *pk_lab_fixture_name(uint16_t species);
const char *pk_lab_tier_name(uint8_t tier);
/* Writes a readable, explicitly simulated outcome. On failure, does
 * not touch destination and never outputs an invented species. */
bool pk_lab_reveal_text(const pk_gacha_result_t *result,
                        char *buffer,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
