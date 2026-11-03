#ifndef POKEMON_TPK2_RENDERER_H
#define POKEMON_TPK2_RENDERER_H
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Reads only Mimikyu #778 Idle (action 0) into a bounded RAM cache.
 * SD is unmounted before returning; no asset is embedded in firmware. */
bool pk_tpk2_idle_load(void);
const lv_img_dsc_t *pk_tpk2_idle_frame(unsigned index);
unsigned pk_tpk2_idle_frame_count(void);
uint16_t pk_tpk2_idle_duration_ms(unsigned index);
void pk_tpk2_idle_release(void);
#ifdef __cplusplus
}
#endif
#endif
