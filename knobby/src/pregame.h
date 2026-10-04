#ifndef _PREGAME_H
#define _PREGAME_H

#include "types.h"

extern lv_obj_t *screen_pregame_home;
extern lv_obj_t *screen_pregame_multiplayer;
extern lv_obj_t *screen_pregame_players;
extern lv_obj_t *screen_pregame_playgroup;
extern lv_obj_t *screen_pregame_member;
extern lv_obj_t *screen_pregame_roster;
extern lv_obj_t *screen_pregame_deck;
extern lv_obj_t *screen_pregame_mulligans;

void build_pregame_screens(void);
void open_pregame_home(void);
void pregame_change_player_count(int delta);
void pregame_change_playgroup(int delta);
void pregame_change_member(int delta);
void pregame_change_deck(int delta);
bool pregame_handle_back(lv_obj_t *screen);

/* Selected commander art for in-game rendering. The descriptor remains valid
   until the next pregame selection/reset. Returns NULL when no art is set. */
const lv_img_dsc_t *pregame_get_player_commander_art(int player_index);

#endif // _PREGAME_H
