#ifndef POKEMON_EVOLUTION_TRANSACTION_H
#define POKEMON_EVOLUTION_TRANSACTION_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_evolution.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Pure transaction coordinator for a LEVEL-only evolution.
 * Item/stone/trade evolutions remain BLOCKED until both wallet and
 * profile can be committed with their own write-ahead transaction.
 * Every callback must use the *same authenticated* user ID. */
typedef enum {
    PK_EVOLVE_TX_FAILED=0,
    PK_EVOLVE_TX_DONE=1,
    PK_EVOLVE_TX_ALREADY_DONE=2,
    PK_EVOLVE_TX_BLOCKED=3,
    PK_EVOLVE_TX_RECOVERY_NEEDED=4
} pk_evolve_tx_result_t;
typedef struct {
    uint32_t user_id;
    uint32_t nonce;          /* unique durable transaction ID; not game ID */
    uint16_t from,to;
    uint8_t team_slot;
    uint8_t reserved[3];
    pk_profile_t before;
    pk_profile_t after;
} pk_evolve_tx_intent_t;
typedef enum {
    PK_EVOLVE_INTENT_ERROR=-2,
    PK_EVOLVE_INTENT_CORRUPT=-1,
    PK_EVOLVE_INTENT_MISSING=0,
    PK_EVOLVE_INTENT_PENDING=1,
    PK_EVOLVE_INTENT_COMMITTED=2
} pk_evolve_intent_status_t;
typedef struct {
    void *ctx;
    pk_evolve_intent_status_t (*intent_read)(void *,uint32_t,pk_evolve_tx_intent_t *);
    bool (*intent_create)(void *,const pk_evolve_tx_intent_t *);
    bool (*intent_complete)(void *,const pk_evolve_tx_intent_t *);
    bool (*profile_read)(void *,uint32_t,pk_profile_t *);
    /* MUST reject a stale before snapshot rather than overwrite it. */
    bool (*profile_save_if_unchanged)(void *,uint32_t,
                const pk_profile_t *before,const pk_profile_t *after);
} pk_evolve_tx_io_t;
/* User has already confirmed a specific target, and the caller has
 * authenticated /me + verified target TPK2 sprite. Never called by
 * background game callbacks. Only a missing intent permits a new tx. */
pk_evolve_tx_result_t pk_evolve_tx_begin(
    const pk_evolve_tx_io_t *io,uint32_t user_id,uint32_t nonce,
    uint8_t slot,uint16_t target);
/* Recovery does not invent consent: only resumes a previously persisted
 * user-confirmed transaction; mismatching profiles fail closed. */
pk_evolve_tx_result_t pk_evolve_tx_recover(
    const pk_evolve_tx_io_t *io,uint32_t user_id);
#ifdef __cplusplus
}
#endif
#endif
