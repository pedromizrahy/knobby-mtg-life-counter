#ifndef POKEMON_ACCOUNT_ONBOARDING_H
#define POKEMON_ACCOUNT_ONBOARDING_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_profile_store.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Account activation is ALWAYS explicit, after a fresh authenticated /me.
 * A valid login may only access its OWN numeric Playgroup user ID.
 * Never derive the ID from a name, selected seat, roster or SD sprites. */
typedef enum {
    PK_ONBOARD_INVALID=0,
    PK_ONBOARD_READY=1,
    PK_ONBOARD_ALREADY_FILE=2,
    PK_ONBOARD_LEGACY_NVS=3,
    PK_ONBOARD_EMPTY_FILE=4,
    PK_ONBOARD_UNSAFE=5
} pk_onboard_state_t;
pk_onboard_state_t pk_onboard_decide(uint64_t verified_me_id,
                                   pk_store_status_t spiffs,
                                   pk_store_status_t nvs);
#ifdef __cplusplus
}
#endif
#endif
