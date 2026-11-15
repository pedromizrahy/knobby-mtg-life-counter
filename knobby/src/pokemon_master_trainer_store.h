#ifndef POKEMON_MASTER_TRAINER_STORE_H
#define POKEMON_MASTER_TRAINER_STORE_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_achievements.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Standalone achievement wallet, NOT included in p->balls[] yet.
 * An unlocked, unspent badge is entitlement to EXACTLY one Master Ball.
 * No gameplay call sites. No automatic creation/award.
 * All calls for the same player must be serialized.
 */
typedef enum {
    PK_MT_ERROR=-2, PK_MT_CORRUPT=-1, PK_MT_MISSING=0, PK_MT_VALID=1
} pk_mt_status_t;
typedef struct {
    bool unlocked;
    bool spent;
    uint32_t unlock_game_id;
} pk_mt_state_t;
pk_mt_status_t pk_mt_inspect(uint32_t user_id, pk_mt_state_t *out);
/* Explicit, confirmed/durable game only. Validates first-100 crossing and
 * session ID in after profile. Fail closed on existing/ambiguous storage.
 * Save FIRST, then display the reward animation.
 */
bool pk_mt_unlock(uint32_t user_id,uint32_t game_id,
                  const pk_profile_t *before,const pk_profile_t *after);
/* Consumes the SINGLE achievement entitlement. It is not a general Pokeball
 * spend API and does not touch the main inventory. Future gacha must use this
 * wallet transactionally with the Pokemon reveal record.
 */
bool pk_mt_mark_spent(uint32_t user_id);
#ifdef __cplusplus
}
#endif
#endif
