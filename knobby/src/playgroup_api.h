#ifndef _PLAYGROUP_API_H
#define _PLAYGROUP_API_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PG_MAX_PLAYGROUPS 8
#define PG_MAX_MEMBERS 32
#define PG_MAX_DECKS 24
#define PG_NAME_LEN 40
#define PG_DECK_NAME_LEN 64
#define PG_COMMANDER_NAME_LEN 64
#define PG_IMAGE_URL_LEN 192
#define PG_SCRYFALL_ID_LEN 40

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
    int games_won;
    int games_lost;
    char last_game_played_at[40];
    char name[PG_DECK_NAME_LEN];
    char commander[PG_COMMANDER_NAME_LEN];
    char partner[PG_COMMANDER_NAME_LEN];
    char art_crop_url[PG_IMAGE_URL_LEN];
    char scryfall_id[PG_SCRYFALL_ID_LEN];
} playgroup_deck_t;

/* USB/Serial provisioning + diagnostics. */
void playgroup_process_serial(void);
bool playgroup_credentials_ready(void);
bool playgroup_prepare_connection(void);

/* On-demand API cache for pregame UI. Wi-Fi stays connected for the whole
   Playgroup setup session and is explicitly released when setup ends. */
bool playgroup_refresh_playgroups(void);
int playgroup_cached_playgroup_count(void);
const playgroup_summary_t *playgroup_cached_playgroup(int index);

bool playgroup_refresh_members(long playgroup_id);
int playgroup_cached_member_count(void);
const playgroup_member_t *playgroup_cached_member(int index);

bool playgroup_refresh_decks(long user_id);
int playgroup_cached_deck_count(void);
const playgroup_deck_t *playgroup_cached_deck(int index);

/* Fetch one public commander image into PSRAM. Caller owns the returned
   buffer and must release it with playgroup_free_image(). */
bool playgroup_download_image(const char *scryfall_id, uint8_t **out_data, size_t *out_size);

/* Prefer the direct art_crop_url already returned by Playgroup. Falls back
   to the Scryfall card-image endpoint when the direct CDN URL fails. */
bool playgroup_download_deck_image(const char *art_crop_url,
                                   const char *scryfall_id,
                                   uint8_t **out_data,
                                   size_t *out_size);

/* Start one low-priority on-demand art fetch without blocking the LVGL task.
   Returns true when the image is already cached or the fetch was started. */
bool playgroup_prefetch_deck_image_async(const char *art_crop_url,
                                         const char *scryfall_id);

void playgroup_free_image(uint8_t *data);

/* Start a low-priority background prefetch for the currently cached deck list.
   Images are kept compressed in PSRAM and reused by the deck picker. */
void playgroup_prefetch_deck_images(void);

/* Non-blocking cache lookup for UI. Returns a caller-owned PSRAM copy. */
bool playgroup_cached_image_copy(const char *scryfall_id,
                                 uint8_t **out_data, size_t *out_size);

/* Deck-aware cache helpers. When Playgroup points the same commander card at
   a different selected art URL, the URL becomes part of the cache identity
   so an older/default printing cannot mask the newly selected artwork. */
bool playgroup_cached_deck_image_copy(const char *art_crop_url,
                                      const char *scryfall_id,
                                      uint8_t **out_data, size_t *out_size);

/* Persist only an art image that is already present in the compressed RAM
   cache. Used after the user actually selects a deck, never during preload. */
bool playgroup_persist_cached_image(const char *scryfall_id);
bool playgroup_persist_cached_deck_image(const char *art_crop_url,
                                         const char *scryfall_id);

bool playgroup_network_active(void);
void playgroup_end_session(void);

#ifdef __cplusplus
}
#endif

#endif // _PLAYGROUP_API_H
