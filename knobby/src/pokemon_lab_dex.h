#ifndef POKEMON_LAB_DEX_H
#define POKEMON_LAB_DEX_H
#include <stddef.h>
#include <stdint.h>
#include "pokemon_gacha.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Proposed offline Kanto+Johto tiers. Not canonical Pokémon rarity.
 * All 251 IDs have user-generated TPK2 on SD in the current hardware test;
 * this catalogue is simulation-only and does not assert ownership. */
#define PK_LAB_DEX_COUNT 251U
const pk_gacha_entry_t *pk_lab_dex_catalog(size_t *count);
const char *pk_lab_dex_name(uint16_t species);
uint8_t pk_lab_dex_tier(uint16_t species);
#ifdef __cplusplus
}
#endif
#endif
