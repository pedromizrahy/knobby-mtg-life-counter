#ifndef POKEMON_UI_H
#define POKEMON_UI_H
#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif
extern lv_obj_t *screen_pokemon_extras;
extern lv_obj_t *screen_pokemon_companion;
/* Build eagerly with the other LVGL screens; no NVS/SPIFFS access. */
void pokemon_ui_build(void);
/* Only reachable from the pregame home. Does not create player data. */
void pokemon_ui_open_extras(void);
void pokemon_ui_open_companion(void);
bool pokemon_ui_handle_back(lv_obj_t *screen);
void pokemon_ui_knob_page(int direction);
#ifdef __cplusplus
}
#endif
#endif
