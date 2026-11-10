#ifndef POKEMON_LAB_RATES_H
#define POKEMON_LAB_RATES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pokemon_lab.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Format the simulated target/observed rates without LVGL. No side effects.
 * False and unchanged output on invalid input or insufficient capacity. */
bool pk_lab_rates_format(const pk_lab_t *lab,uint8_t ball,
                         char *out,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
