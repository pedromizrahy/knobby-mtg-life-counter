#ifndef COMMANDER_IMAGE_DECODE_H
#define COMMANDER_IMAGE_DECODE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool commander_image_decode_rgb565(const uint8_t *jpeg, size_t jpeg_size,
                                   uint8_t **out_pixels,
                                   uint16_t *out_width,
                                   uint16_t *out_height);

void commander_image_free_pixels(uint8_t *pixels);

#ifdef __cplusplus
}
#endif

#endif
