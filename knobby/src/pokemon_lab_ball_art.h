#ifndef POKEMON_LAB_BALL_ART_H
#define POKEMON_LAB_BALL_ART_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original procedural pixel illustration, no external Pokemon artwork.
 * 32x32 logical pixels at 2x, RGB565 followed by 8-bit opacity per pixel.
 * Frames 0..11: hold, four shakes, separation, open, glow and sparkles.
 * Strictly visual: no RNG, inventory, NVS/SPIFFS or Playgroup access. */
#define PK_LAB_BALL_PIXELS 64U
#define PK_LAB_BALL_FRAMES 12U
#define PK_LAB_BALL_BUFFER_BYTES (PK_LAB_BALL_PIXELS*PK_LAB_BALL_PIXELS*3U)
bool pk_lab_ball_art_render(uint8_t ball,unsigned frame,
                            uint8_t *dst,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
