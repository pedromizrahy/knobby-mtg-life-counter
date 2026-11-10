#ifndef POKEMON_STARTER_UI_H
#define POKEMON_STARTER_UI_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Opens only from a Playgroup PARCEIROS card, after a fresh verified /me
 * matches the global user ID of that card. Never auto-creates a starter. */
bool pk_starter_ui_open(uint32_t expected_playgroup_user_id);
#ifdef __cplusplus
}
#endif
#endif
