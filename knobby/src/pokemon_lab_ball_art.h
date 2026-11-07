#ifndef POKEMON_LAB_BALL_ART_H
#define POKEMON_LAB_BALL_ART_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original 64x64 RGB565 + 8-bit opacity scene, 18 animation frames:
 * latch pulse, four controlled shakes, opening shell and a restrained flash.
 * No external artwork, filesystem access or Playgroup writes. */
#define PK_LAB_BALL_PIXELS 64U
#define PK_LAB_BALL_FRAMES 18U
#define PK_LAB_BALL_BUFFER_BYTES (PK_LAB_BALL_PIXELS*PK_LAB_BALL_PIXELS*3U)
bool pk_lab_ball_art_render(uint8_t ball,unsigned frame,
                            uint8_t *dst,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
