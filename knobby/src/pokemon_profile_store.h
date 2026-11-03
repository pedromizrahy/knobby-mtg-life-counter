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
typedef enum {
    PK_STORE_ERROR = -2,
    PK_STORE_CORRUPT = -1,
    PK_STORE_MISSING = 0,
    PK_STORE_VALID = 1
} pk_store_status_t;
/* Read-only inspection, distinguishing genuinely empty slots from corrupt data. */
pk_store_status_t pk_store_inspect(uint32_t user_id, pk_profile_t *out);
bool pk_store_load(uint32_t user_id, pk_profile_t *out);
/* Explicit creation only; never overwrites existing or unreadable records.
 * The starter species is currently Mimikyu (#778), not a random roll.
 * Players are identified by Playgroup's global numeric user ID. */
bool pk_store_create_starter(uint32_t user_id);
bool pk_store_save(uint32_t user_id, const pk_profile_t *profile);
/* Explicit diagnostic only: uses a reserved test ID, never real users. */
bool pk_store_selftest(void);
#ifdef __cplusplus
}
#endif
#endif
