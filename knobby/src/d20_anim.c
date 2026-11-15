#include "d20_anim.h"
#include "esp_heap_caps.h"
#include <stdint.h>
#include <string.h>
#if __has_include("dice_d20_frames.h")
#include "dice_d20_frames.h"
#define D20_HAS_FRAMES 1
#else
#define D20_HAS_FRAMES 0
#endif
static lv_obj_t *s_image = NULL;
static uint8_t *s_pixels = NULL;
static lv_img_dsc_t s_descriptor;
bool d20_anim_available(void) { return D20_HAS_FRAMES != 0; }
void d20_anim_hide(void)
{
    if (s_image) lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
}
void d20_anim_show(lv_obj_t *parent, unsigned frame, int result)
{
#if D20_HAS_FRAMES
    if (!parent) return;
    if (!s_pixels) {
        s_pixels = (uint8_t *)heap_caps_malloc(D20_FRAME_WIDTH * D20_FRAME_HEIGHT * 2U,
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_pixels) return;
        memset(&s_descriptor, 0, sizeof(s_descriptor));
        s_descriptor.header.always_zero = 0;
        s_descriptor.header.w = D20_FRAME_WIDTH;
        s_descriptor.header.h = D20_FRAME_HEIGHT;
        s_descriptor.header.cf = LV_IMG_CF_TRUE_COLOR;
        s_descriptor.data_size = D20_FRAME_WIDTH * D20_FRAME_HEIGHT * 2U;
        s_descriptor.data = s_pixels;
    }
    if (!s_image) {
        s_image = lv_img_create(parent);
        lv_obj_add_flag(s_image, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_img_set_src(s_image, &s_descriptor);
        lv_obj_align(s_image, LV_ALIGN_CENTER, 0, 0);
    }
    unsigned index = frame < D20_ROLL_FRAME_COUNT ? frame :
                     (unsigned)(D20_ROLL_FRAME_COUNT +
                     ((result > 0 && result <= 20) ? result - 1 : 0));
    uint32_t pos = d20_frame_offsets[index];
    uint32_t end = pos + d20_frame_lengths[index];
    uint32_t pixel = 0;
    while (pos + 2U < end && pixel < D20_FRAME_WIDTH * D20_FRAME_HEIGHT) {
        uint8_t run = d20_frame_stream[pos++];
        uint16_t color = (uint16_t)d20_frame_stream[pos++] |
                         ((uint16_t)d20_frame_stream[pos++] << 8);
        for (unsigned i = 0; i < run && pixel < D20_FRAME_WIDTH * D20_FRAME_HEIGHT; ++i, ++pixel) {
            /* The firmware uses LV_COLOR_16_SWAP=1. */
            s_pixels[pixel * 2U] = (uint8_t)(color >> 8);
            s_pixels[pixel * 2U + 1U] = (uint8_t)color;
        }
    }
    lv_img_cache_invalidate_src(&s_descriptor);
    lv_img_set_src(s_image, &s_descriptor);
    lv_obj_align(s_image, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_image);
#else
    (void)parent; (void)frame; (void)result;
#endif
}
