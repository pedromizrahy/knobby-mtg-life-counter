#ifndef PK_MOCK_ESP_RANDOM_H
#define PK_MOCK_ESP_RANDOM_H
#include <stdint.h>
static inline uint32_t esp_random(void) {
    static uint32_t value=0x12345678U;
    value=value*1664525U+1013904223U;
    return value;
}
#endif
