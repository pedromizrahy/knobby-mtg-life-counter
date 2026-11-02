#ifndef POKEMON_PROFILE_STORE_H
#define POKEMON_PROFILE_STORE_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_progress.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Playgroup numeric user ID, NOT a seat number. All functions are opt-in;
 * no game path calls them until integration is explicitly enabled. */
bool pk_store_load(uint32_t user_id, pk_profile_t *out);
bool pk_store_save(uint32_t user_id, const pk_profile_t *profile);
#ifdef __cplusplus
}
#endif
#endif
