#ifndef _DIAL_SPRITE_H
#define _DIAL_SPRITE_H
#include "lvgl.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 0=coin, 1=D4, 2=D6, 3=D8, 4=D10, 5=D12,
 * 6=D20 (existing player), 7=D100, 99=intro */
bool dial_sprite_available(int kind);
void dial_sprite_show(lv_obj_t *parent, int kind, unsigned roll_frame, int result);
void dial_sprite_hide(void);
#ifdef __cplusplus
}
#endif
#endif
