#ifndef POKEMON_ACHIEVEMENTS_H
#define POKEMON_ACHIEVEMENTS_H

#include <stdbool.h>
#include "pokemon_progress.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure detection only; grants neither badges nor Master Balls.
 * Caller must use a DURABLE once-per-player unlock and an atomic/idempotent
 * reward claim before enabling this on real Playgroup results.
 * These snapshots must come from the same player and confirmed game.
 */
bool pk_master_trainer_first_level_100(const pk_profile_t *before,
                                       const pk_profile_t *after);

#ifdef __cplusplus
}
#endif
#endif
