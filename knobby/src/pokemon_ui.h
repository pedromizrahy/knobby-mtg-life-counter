#ifndef POKEMON_UI_H
#define POKEMON_UI_H
#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif
extern lv_obj_t *screen_pokemon_companion;
/* Build eagerly with the other LVGL screens; no NVS/SPIFFS access. */
void pokemon_ui_build(void);
/* Opened directly from the pregame home. Does not create player data. */
void pokemon_ui_open_companion(void);
/* Explicit selection for a species with a user-installed TPK2 sprite.
 * Does not change Pokédex ownership, team, Playgroup data or rewards.
 * Failure restores the existing Mimikyu default if available.
 * Call only on the LVGL/UI thread, outside active game screens. */
bool pokemon_ui_show_species(uint16_t species);
bool pokemon_ui_handle_back(lv_obj_t *screen);
void pokemon_ui_knob_page(int direction);
#ifdef __cplusplus
}
#endif
#endif
