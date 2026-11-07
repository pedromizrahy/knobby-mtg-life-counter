#ifndef POKEMON_LAB_BALL_ART_H
#define POKEMON_LAB_BALL_ART_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pixel-art ball on a transparent 96x96 native RGB565+alpha canvas.
 * 18 frames: anticipation, controlled shakes, latch glow and cap opening.
 * No external artwork or persistent state changes. */
#define PK_LAB_BALL_PIXELS 96U
#define PK_LAB_BALL_FRAMES 18U
#define PK_LAB_BALL_BUFFER_BYTES (PK_LAB_BALL_PIXELS*PK_LAB_BALL_PIXELS*3U)
bool pk_lab_ball_art_render(uint8_t ball,unsigned frame,
                            uint8_t *dst,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
