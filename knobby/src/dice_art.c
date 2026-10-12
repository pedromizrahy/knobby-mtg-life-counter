#include "dice_art.h"
#include <string.h>
#include "esp_heap_caps.h"

/* The images are an optional local asset header. The runtime correction
 * below fixes RGB565 byte order for LV_COLOR_16_SWAP=1 and wipes residual
 * alpha from the former pale square image background. */
#if __has_include("dice_assets.h")
#include "dice_assets.h"
#define DIAL_HAS_ASSETS 1
#else
#define DIAL_HAS_ASSETS 0
#endif

#if DIAL_HAS_ASSETS
static const lv_img_dsc_t *originals[9] = {
    &coin_heads_img, &coin_tails_img, &die_d4_img,
    &die_d6_img, &die_d8_img, &die_d10_img,
    &die_d12_img, &die_d20_img, &die_d100_img
};
static lv_img_dsc_t converted[9];
static uint8_t *buffers[9] = {0};
#endif

const lv_img_dsc_t *dial_art_get(int index)
{
#if DIAL_HAS_ASSETS
    const lv_img_dsc_t *src;
    uint8_t *dst;
    uint32_t i;
    if (index < 0 || index >= 9) return NULL;
    if (buffers[index]) return &converted[index];
    src = originals[index];
    if (!src || src->data_size != (uint32_t)src->header.w * src->header.h * 3U)
        return NULL;
    dst = (uint8_t *)heap_caps_malloc(src->data_size,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!dst) return NULL;
    for (i = 0; i + 2U < src->data_size; i += 3U) {
        /* Original header was made for unswapped 16-bit pixels. */
        dst[i] = src->data[i + 1U];
        dst[i + 1U] = src->data[i];
        dst[i + 2U] = src->data[i + 2U] < 90U ? 0U : src->data[i + 2U];
    }
    converted[index] = *src;
    converted[index].data = dst;
    buffers[index] = dst;
    return &converted[index];
#else
    (void)index;
    return NULL;
#endif
}
