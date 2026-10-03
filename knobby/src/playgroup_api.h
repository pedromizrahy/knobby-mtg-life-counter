#ifndef _PLAYGROUP_API_H
#define _PLAYGROUP_API_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PG_MAX_PLAYGROUPS 8
#define PG_MAX_MEMBERS 32
#define PG_MAX_DECKS 24
#define PG_NAME_LEN 40
#define PG_DECK_NAME_LEN 64
#define PG_COMMANDER_NAME_LEN 64

typedef struct {
    long id;
    int member_count;
    char name[PG_NAME_LEN];
} playgroup_summary_t;

typedef struct {
    long user_id;
    bool admin;
    char username[PG_NAME_LEN];
} playgroup_member_t;

typedef struct {
    long id;
    long user_id;
    bool archived;
    float power_level;
    char name[PG_DECK_NAME_LEN];
    char commander[PG_COMMANDER_NAME_LEN];
    char partner[PG_COMMANDER_NAME_LEN];
} playgroup_deck_t;

/* USB/Serial provisioning + diagnostics. */
void playgroup_process_serial(void);
bool playgroup_credentials_ready(void);

/* On-demand API cache for pregame UI. These calls connect Wi-Fi, perform
   verified HTTPS requests, populate bounded RAM caches, then turn Wi-Fi off. */
bool playgroup_refresh_playgroups(void);
int playgroup_cached_playgroup_count(void);
const playgroup_summary_t *playgroup_cached_playgroup(int index);

bool playgroup_refresh_members(long playgroup_id);
int playgroup_cached_member_count(void);
const playgroup_member_t *playgroup_cached_member(int index);

bool playgroup_refresh_decks(long user_id);
int playgroup_cached_deck_count(void);
const playgroup_deck_t *playgroup_cached_deck(int index);

#ifdef __cplusplus
}
#endif

#endif // _PLAYGROUP_API_H
