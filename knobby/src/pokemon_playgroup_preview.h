#ifndef POKEMON_PLAYGROUP_PREVIEW_H
#define POKEMON_PLAYGROUP_PREVIEW_H
#include "pokemon_playgroup_roster.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Opt-in READ-ONLY presentation in the pregame Playgroup roster.
 * Max four visible at once; page for 5th/6th player. No account writes. */
bool pk_pg_preview_open(const pk_pg_seat_t *seats,size_t count);
#ifdef __cplusplus
}
#endif
#endif
