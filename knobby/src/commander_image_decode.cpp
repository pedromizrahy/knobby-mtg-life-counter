#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <lvgl.h>

#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_MALLOC(sz) heap_caps_malloc((sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define STBI_REALLOC(p,newsz) heap_caps_realloc((p), (newsz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define STBI_FREE(p) heap_caps_free((p))
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "commander_image_decode.h"

static uint16_t rgb888_to_lvgl565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t color = (uint16_t)(((uint16_t)(r & 0xF8U) << 8) |
                                ((uint16_t)(g & 0xFCU) << 3) |
                                ((uint16_t)b >> 3));
#if LV_COLOR_16_SWAP
    color = (uint16_t)((color << 8) | (color >> 8));
#endif
    return color;
}

bool commander_image_decode_rgb565(const uint8_t *jpeg, size_t jpeg_size,
                                   uint8_t **out_pixels,
                                   uint16_t *out_width,
                                   uint16_t *out_height)
{
    int src_w = 0;
    int src_h = 0;
    int src_comp = 0;
    unsigned char *rgb = NULL;
    uint8_t *dst = NULL;
    uint16_t dst_w;
    uint16_t dst_h;
    size_t dst_bytes;
    /* Background art: enough detail for a 360 px display without decoding
       the full Scryfall image into PSRAM. */
    const uint16_t max_w = 300;
    const uint16_t max_h = 220;

    if (!jpeg || jpeg_size == 0 || !out_pixels || !out_width || !out_height)
        return false;

    *out_pixels = NULL;
    *out_width = 0;
    *out_height = 0;

    if (!stbi_info_from_memory(jpeg, (int)jpeg_size, &src_w, &src_h, &src_comp)) {
        Serial.print("[Playgroup] stb JPEG info failed: ");
        Serial.println(stbi_failure_reason());
        return false;
    }

    Serial.print("[Playgroup] stb JPEG source ");
    Serial.print(src_w);
    Serial.print("x");
    Serial.print(src_h);
    Serial.print("; components ");
    Serial.println(src_comp);

    rgb = stbi_load_from_memory(jpeg, (int)jpeg_size, &src_w, &src_h, &src_comp, 3);
    if (!rgb) {
        Serial.print("[Playgroup] stb JPEG decode failed: ");
        Serial.println(stbi_failure_reason());
        return false;
    }

    if (src_w <= 0 || src_h <= 0) {
        stbi_image_free(rgb);
        return false;
    }

    {
        float scale_w = (float)max_w / (float)src_w;
        float scale_h = (float)max_h / (float)src_h;
        float scale = scale_w < scale_h ? scale_w : scale_h;
        if (scale > 1.0f) scale = 1.0f;
        dst_w = (uint16_t)((float)src_w * scale);
        dst_h = (uint16_t)((float)src_h * scale);
        if (dst_w < 1) dst_w = 1;
        if (dst_h < 1) dst_h = 1;
    }

    dst_bytes = (size_t)dst_w * (size_t)dst_h * 2U;
    dst = (uint8_t *)heap_caps_malloc(dst_bytes,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!dst) {
        Serial.print("[Playgroup] Commander RGB565 allocation failed: ");
        Serial.print((unsigned)dst_bytes);
        Serial.println(" bytes");
        stbi_image_free(rgb);
        return false;
    }

    for (uint16_t y = 0; y < dst_h; y++) {
        int sy = ((int)y * src_h) / dst_h;
        for (uint16_t x = 0; x < dst_w; x++) {
            int sx = ((int)x * src_w) / dst_w;
            size_t src_i = ((size_t)sy * (size_t)src_w + (size_t)sx) * 3U;
            size_t dst_i = ((size_t)y * (size_t)dst_w + (size_t)x) * 2U;
            uint16_t color = rgb888_to_lvgl565(rgb[src_i], rgb[src_i + 1], rgb[src_i + 2]);
            dst[dst_i] = (uint8_t)(color & 0xFFU);
            dst[dst_i + 1] = (uint8_t)(color >> 8);
        }
    }

    stbi_image_free(rgb);

    *out_pixels = dst;
    *out_width = dst_w;
    *out_height = dst_h;

    Serial.print("[Playgroup] Commander JPEG decoded to ");
    Serial.print(dst_w);
    Serial.print("x");
    Serial.print(dst_h);
    Serial.print(" RGB565; ");
    Serial.print((unsigned)dst_bytes);
    Serial.println(" bytes");

    return true;
}

void commander_image_free_pixels(uint8_t *pixels)
{
    if (pixels)
        heap_caps_free(pixels);
}
