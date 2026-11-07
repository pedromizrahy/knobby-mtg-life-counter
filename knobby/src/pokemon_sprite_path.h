#ifndef POKEMON_SPRITE_PATH_H
#define POKEMON_SPRITE_PATH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Only decimal National Dex IDs, never user-provided paths.
 * Destination includes NUL. No storage or hardware side effects. */
bool pk_sprite_path(uint16_t species,char *buffer,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
