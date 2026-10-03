// Compatibility wrapper based on Espressif esp32-camera JPEG decoder.
// SPDX-License-Identifier: Apache-2.0
#ifndef KNOBBY_ESP_JPG_DECODE_COMPAT_H
#define KNOBBY_ESP_JPG_DECODE_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KNOBBY_JPG_SCALE_NONE,
    KNOBBY_JPG_SCALE_2X,
    KNOBBY_JPG_SCALE_4X,
    KNOBBY_JPG_SCALE_8X,
} knobby_jpg_scale_t;

typedef size_t (*knobby_jpg_reader_cb)(void *arg, size_t index, uint8_t *buf, size_t len);
typedef bool (*knobby_jpg_writer_cb)(void *arg, uint16_t x, uint16_t y,
                                     uint16_t w, uint16_t h, uint8_t *data);

esp_err_t knobby_esp_jpg_decode(size_t len, knobby_jpg_scale_t scale,
                                knobby_jpg_reader_cb reader,
                                knobby_jpg_writer_cb writer,
                                void *arg);

#ifdef __cplusplus
}
#endif
#endif
