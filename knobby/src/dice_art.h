#ifndef DIAL_ART_H
#define DIAL_ART_H
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 0=Heads 1=Tails 2=D4 3=D6 4=D8 5=D10 6=D12 7=D20 8=D100 */
const lv_img_dsc_t *dial_art_get(int index);
#ifdef __cplusplus
}
#endif
#endif
