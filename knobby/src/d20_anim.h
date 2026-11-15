#ifndef _D20_ANIM_H
#define _D20_ANIM_H
#include "lvgl.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool d20_anim_available(void);
void d20_anim_show(lv_obj_t *parent, unsigned frame, int result);
void d20_anim_hide(void);
#ifdef __cplusplus
}
#endif
#endif
