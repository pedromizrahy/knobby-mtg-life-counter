#ifndef _PLAYGROUP_PENDING_H
#define _PLAYGROUP_PENDING_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PG_PENDING_MAX_EVENTS 128
#define PG_PENDING_MAX_QUEUE 16

typedef struct {
    long user_id;
    long deck_id;
    uint8_t mulligans;
    char name[16];
} pg_pending_player_seed_t;

typedef struct {
    long playgroup_id;
    uint8_t player_count;
    pg_pending_player_seed_t players[MAX_DISPLAY_PLAYERS];
} pg_pending_seed_t;

/* Start tracking a Playgroup-backed game after the game state has been reset. */
void playgroup_pending_begin_game(const pg_pending_seed_t *seed);

/* Disable tracking for a local/non-Playgroup game without touching older queue files. */
void playgroup_pending_disable_current(void);

/* Re-evaluate the current game's outcome after elimination/revival changes. */
void playgroup_pending_reconcile_outcome(void);

/* Diagnostics for the persistent queue. */
int playgroup_pending_count(void);
void playgroup_pending_print_status(void);

#ifdef __cplusplus
}
#endif

#endif // _PLAYGROUP_PENDING_H
