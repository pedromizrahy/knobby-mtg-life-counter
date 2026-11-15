#ifndef POKEMON_COMPANION_PROFILE_VIEW_H
#define POKEMON_COMPANION_PROFILE_VIEW_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_progress.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Pure read-only bridge. The caller MUST supply a previously authenticated
 * Playgroup numeric user ID; never a seat, a guest slot or a guessed ID.
 * Does not create users, mutate profiles, touch storage or open a sprite.
 */
typedef bool (*pk_companion_profile_load_t)(void *ctx,uint32_t user_id,
                                           pk_profile_t *out);
typedef struct {
    uint32_t user_id;
    uint16_t species;
    uint8_t level;
    uint8_t shiny;
    uint32_t xp;
} pk_companion_view_t;
/* Caller supplies a trusted loader (e.g., pk_store_load) and authenticated
 * ID. Returns false if the user is absent, the species isn't in the team,
 * or the stored profile is malformed. Never overwrites output on failure.
 * Duplicate species resolve to first occupied team slot until a box exists.
 */
bool pk_companion_read_view(pk_companion_profile_load_t load,void *ctx,
                            uint32_t user_id,uint16_t species,
                            pk_companion_view_t *out);
#ifdef __cplusplus
}
#endif
#endif
