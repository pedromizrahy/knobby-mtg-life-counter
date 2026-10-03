#ifndef _PREGAME_H
#define _PREGAME_H

#include "types.h"

extern lv_obj_t *screen_pregame_home;
extern lv_obj_t *screen_pregame_multiplayer;
extern lv_obj_t *screen_pregame_players;
extern lv_obj_t *screen_pregame_roster;
extern lv_obj_t *screen_pregame_mulligans;

void build_pregame_screens(void);
void open_pregame_home(void);
bool pregame_handle_back(lv_obj_t *screen);

#endif // _PREGAME_H
