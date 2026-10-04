#ifndef _UI_MP_H
#define _UI_MP_H

#include "types.h"

// ---------- screens ----------
extern lv_obj_t *screen_multiplayer;

// ---------- functions ----------
void build_multiplayer_screen(void);
void rebuild_multiplayer_layout(int track);

void refresh_multiplayer_ui(void);
void refresh_multiplayer_life_preview(void);
void refresh_multiplayer_selection_animation(void);
void refresh_multiplayer_turn_ui(void);

int mp_player_seat_rotation(int player);

void select_kick_timer(void);
void multiplayer_cancel_damage_drag(void);
bool multiplayer_damage_drag_active(void);

#endif // _UI_MP_H
