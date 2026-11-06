#ifndef POKEMON_MASTER_TRAINER_INTENT_H
#define POKEMON_MASTER_TRAINER_INTENT_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_progress.h"
#include "pokemon_reward_ledger.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Opt-in WAL for the unique Master Trainer achievement.
 * PREPARE: must happen BEFORE the associated profile save.
 * FINALIZE: must happen AFTER the session is durably COMMITTED.
 * No automatic invocation from game/postgame; separate from main rewards.
 * Each player's operations must be serialized.
 */
typedef enum {
    PK_MT_INTENT_ERROR=-2, PK_MT_INTENT_CONFLICT=-1,
    PK_MT_INTENT_MISSING=0, PK_MT_INTENT_VALID=1
} pk_mt_intent_status_t;
typedef struct {
    void *ctx;
    pk_ledger_state_t (*get_ledger)(void *,uint32_t,uint32_t);
    bool (*load_profile)(void *,uint32_t,pk_profile_t *);
} pk_mt_recovery_io_t;
pk_mt_intent_status_t pk_mt_intent_status(uint32_t user_id,uint32_t *game_id);
/* Creates durable, immutable proof BEFORE any profile mutation. */
bool pk_mt_intent_prepare(uint32_t user_id,uint32_t game_id,
                          const pk_profile_t *before,const pk_profile_t *after);
/* Reconciles proof with both the committed ledger and persisted profile.
 * Returns true also for a previously completed claim of this same session.
 * On any uncertain state it refuses to issue currency.
 */
bool pk_mt_intent_finalize(uint32_t user_id,uint32_t game_id,
                           const pk_mt_recovery_io_t *io);
#ifdef __cplusplus
}
#endif
#endif
