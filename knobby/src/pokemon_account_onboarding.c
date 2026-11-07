#include "pokemon_account_onboarding.h"
#include "pokemon_player_identity.h"
pk_onboard_state_t pk_onboard_decide(uint64_t id,
                                   pk_store_status_t file,
                                   pk_store_status_t nvs)
{
    if(!pk_player_id_valid(id))return PK_ONBOARD_INVALID;
    /* A corrupt or unreadable profile is NEVER considered empty. */
    if(file==PK_STORE_CORRUPT || file==PK_STORE_ERROR ||
       nvs==PK_STORE_CORRUPT || nvs==PK_STORE_ERROR)
        return PK_ONBOARD_UNSAFE;
    if(file==PK_STORE_VALID)return PK_ONBOARD_ALREADY_FILE;
    if(nvs==PK_STORE_VALID)return PK_ONBOARD_LEGACY_NVS;
    if(file==PK_STORE_MISSING && nvs==PK_STORE_MISSING)
        return PK_ONBOARD_READY;
    return PK_ONBOARD_UNSAFE;
}
