#ifndef _UI_DAMAGE_RESOLVER_H
#define _UI_DAMAGE_RESOLVER_H

#include "types.h"

extern lv_obj_t *screen_damage_target;
extern lv_obj_t *screen_damage_resolver;

void build_damage_resolver_screens(void);
void open_damage_resolver(int source_player);
void refresh_damage_resolver_ui(void);
void damage_resolver_change_amount(int delta);

#endif // _UI_DAMAGE_RESOLVER_H
