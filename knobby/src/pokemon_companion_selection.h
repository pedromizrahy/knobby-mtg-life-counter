#ifndef POKEMON_COMPANION_SELECTION_H
#define POKEMON_COMPANION_SELECTION_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_progress.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Pure team selection. A sprite on SD or a Dex discovery flag is not
 * evidence of ownership. Only team slots are registered possessions in
 * profile v1; a future persistent box needs its own selection model.
 * The caller must separately check the requested TPK2 sprite.
 * This is not a profile write, team change or a postgame reward.
 */
bool pk_companion_is_owned(const pk_profile_t *profile,uint16_t species);
/* Find next team species in National Dex order, wrapping at 1 / 1025.
 * direction must be +1 or -1; from=0 starts before first (forward)
 * or after last (reverse). Never selects a species outside the team.
 * Returns 0 for an empty collection or invalid profile/input.
 */
uint16_t pk_companion_next_owned(const pk_profile_t *profile,
                                  uint16_t from,int direction);
#ifdef __cplusplus
}
#endif
#endif
