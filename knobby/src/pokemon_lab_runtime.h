#ifndef POKEMON_LAB_RUNTIME_H
#define POKEMON_LAB_RUNTIME_H
#include "pokemon_lab.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One RAM-only lab shared by USB diagnostics and the visual test screen.
 * Per-call mutex serializes both UI and serial commands; no Playgroup ID,
 * storage, wallet, SD or real profile is ever read or written. */
bool pk_lab_runtime_start(void);
bool pk_lab_runtime_reset(void);
bool pk_lab_runtime_snapshot(pk_lab_t *out);
bool pk_lab_runtime_draw(uint8_t ball,unsigned count);
/* Draw and capture result/stats under the SAME lock. This ensures the
 * visual reveal cannot accidentally display a concurrent serial draw. */
bool pk_lab_runtime_draw_snapshot(uint8_t ball,unsigned count,pk_lab_t *out);
/* A monotonically incrementing RAM session token. START and RESET bump
 * it, but ordinary draws do not. Use the atomic draw-token API to avoid
 * revealing a stale result after a serial reset during LVGL animation. */
bool pk_lab_runtime_draw_snapshot_token(uint8_t ball,unsigned count,
                                        pk_lab_t *out,uint32_t *token);
bool pk_lab_runtime_session_token(uint32_t *token);
bool pk_lab_runtime_badge(void);
bool pk_lab_runtime_level(uint8_t level);
#ifdef __cplusplus
}
#endif
#endif
