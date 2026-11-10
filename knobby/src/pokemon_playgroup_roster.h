#ifndef POKEMON_PLAYGROUP_ROSTER_H
#define POKEMON_PLAYGROUP_ROSTER_H
#include "pokemon_progress.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Read-only view of explicitly selected Playgroup seats.
 * A username/seat is never used as a user ID. No profile is created. */
typedef struct {
    uint64_t user_id; /* 0 = guest/unmapped */
    char name[16];
} pk_pg_seat_t;
typedef struct {
    char name[16];
    uint32_t user_id;
    uint16_t species;
    uint8_t shiny,level;
    uint32_t xp;
    bool has_main;
} pk_pg_main_t;
typedef bool (*pk_pg_load_profile_t)(void *ctx,uint32_t user_id,pk_profile_t *out);
/* Views are ordered by seat. All invalid/duplicate IDs fail closed.
 * Missing profile and empty team are explicitly NOT LINKED; no SD asset
 * or Dex discovery can manufacture a main Pokémon. */
bool pk_pg_collect_main(const pk_pg_seat_t *seats,size_t count,
                        pk_pg_load_profile_t load,void *ctx,
                        pk_pg_main_t *out,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
