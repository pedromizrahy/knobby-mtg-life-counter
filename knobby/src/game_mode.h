#ifndef _GAME_MODE_H
#define _GAME_MODE_H

#include "types.h"

// ---------- screens ----------
extern lv_obj_t *screen_game_mode_menu;
extern lv_obj_t *screen_game_setup;
extern lv_obj_t *screen_game_player_settings;
extern lv_obj_t *screen_custom_life;

// ---------- functions ----------
void build_game_mode_menu_screen(void);
void build_custom_life_screen(void);
void refresh_game_mode_menu_ui(void);
void refresh_custom_life_ui(void);
void open_game_mode_menu(void);
void return_to_game_mode_menu(void);
void change_custom_life(int delta);

#endif // _GAME_MODE_H
