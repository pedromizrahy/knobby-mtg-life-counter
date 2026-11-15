#ifndef POKEMON_LAB_FIXTURE_ART_H
#define POKEMON_LAB_FIXTURE_ART_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original 40x40 RGB565 + 8-bit alpha tier/species emblems.
 * NOT official Pokemon art. Pure, no filesystem, RNG or player state. */
#define PK_LAB_FIXTURE_PIXELS 40U
#define PK_LAB_FIXTURE_BUFFER_BYTES (40U*40U*3U)
bool pk_lab_fixture_art(uint16_t species,bool shiny,uint8_t *out,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
