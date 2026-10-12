#ifndef _POSTGAME_H
#define _POSTGAME_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_obj_t *screen_postgame_winner;
extern lv_obj_t *screen_postgame_wincon;
extern lv_obj_t *screen_postgame_infinite;

void build_postgame_screens(void);
void postgame_change_selection(int delta);

#ifdef __cplusplus
}
#endif

#endif // _POSTGAME_H
