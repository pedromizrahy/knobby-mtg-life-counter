#ifndef POKEMON_STARTER_SERVICE_H
#define POKEMON_STARTER_SERVICE_H
#include "pokemon_starter_store.h"
#include "pokemon_profile_store.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The caller MUST fresh-authenticate /me and prove the selected ID is
 * exactly that account. Never call for another seat or a guest. */
typedef enum {
    PK_STARTER_FAILED=0,
    PK_STARTER_REVEALED=1,
    PK_STARTER_ACCEPTED=2,
    PK_STARTER_ALREADY_OWNED=3,
    PK_STARTER_BLOCKED=4
} pk_starter_result_t;
pk_starter_result_t pk_starter_service_open(
    uint32_t verified_user_id,const pk_gacha_entry_t *catalog,size_t count,
    pk_gacha_rng_t rng,void *rng_ctx,pk_starter_state_t *out);
pk_starter_result_t pk_starter_service_reroll(
    uint32_t verified_user_id,const pk_gacha_entry_t *catalog,size_t count,
    pk_gacha_rng_t rng,void *rng_ctx,pk_starter_state_t *out);
pk_starter_result_t pk_starter_service_keep(
    uint32_t verified_user_id,pk_starter_state_t *out);
#ifdef __cplusplus
}
#endif
#endif
