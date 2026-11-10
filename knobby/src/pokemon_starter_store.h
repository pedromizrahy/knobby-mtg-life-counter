#ifndef POKEMON_STARTER_STORE_H
#define POKEMON_STARTER_STORE_H
#include <stdint.h>
#include "pokemon_starter_roll.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Two slots, fail-closed if either present slot is corrupt or unreadable.
 * Never roll back to an older generation to grant extra rerolls.
 * Records remain after KEEP as durable proof of the chosen result. */
typedef enum {
    PK_STARTER_STORE_ERROR=-2,
    PK_STARTER_STORE_CORRUPT=-1,
    PK_STARTER_STORE_MISSING=0,
    PK_STARTER_STORE_VALID=1
} pk_starter_store_status_t;
pk_starter_store_status_t pk_starter_store_inspect(uint32_t user_id,
                                                    pk_starter_state_t *out);
bool pk_starter_store_create(uint32_t user_id,
                             const pk_starter_state_t *state);
bool pk_starter_store_update(uint32_t user_id,
                             const pk_starter_state_t *state);
/* Temporary test command backing; ONLY 184199, non-KEEP record.
 * Caller must authenticate /me and inspect both profile stores first. */
bool pk_starter_store_reset_test(uint32_t user_id);
#ifdef __cplusplus
}
#endif
#endif
