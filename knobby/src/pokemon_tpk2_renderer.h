#ifndef POKEMON_TPK2_RENDERER_H
#define POKEMON_TPK2_RENDERER_H
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Read-only bounded Idle sprite cache; no asset is embedded in firmware.
 * Caller must stop animations and release LVGL references before switching
 * to another species, because the frame cache is reused.
 * Existing Mimikyu entry point remains the default. */
bool pk_tpk2_idle_load_species(uint16_t species);
bool pk_tpk2_idle_load(void);
uint16_t pk_tpk2_idle_loaded_species(void);
const lv_img_dsc_t *pk_tpk2_idle_frame(unsigned index);
unsigned pk_tpk2_idle_frame_count(void);
uint16_t pk_tpk2_idle_duration_ms(unsigned index);
void pk_tpk2_idle_release(void);
/* One read-only SD scan for installed Kanto/Johto sprites plus Mimikyu.
 * Does not decode sprites or change the cached Idle animation. Caller
 * supplies >=252 entries; return count, 0 if SD unavailable. */
size_t pk_tpk2_list_installed_gen12(uint16_t *out,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
