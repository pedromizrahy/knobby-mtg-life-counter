#ifndef POKEMON_PROGRESS_H
#define POKEMON_PROGRESS_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Phase 1: isolated, platform-independent game rules.
 * No LVGL, network, SPIFFS or Magic state is touched.
 * Persistence and postgame hookup are separate, gated integration steps. */
#define PK_TEAM_MAX 6
#define PK_DEX_SPECIES 1025
#define PK_DEX_BYTES ((PK_DEX_SPECIES+7)/8)
#define PK_RECENT_GAMES 32
enum { PK_BALL_POKE=0, PK_BALL_GREAT=1, PK_BALL_ULTRA=2, PK_BALL_MASTER=3, PK_BALL_COUNT=4 };
typedef struct {
    uint16_t species; /* 1..1025, 0 for empty slot */
    uint8_t shiny;
    uint8_t level; /* 1..100 */
    uint32_t xp;
} pk_mon_t;
typedef struct {
    uint16_t version;
    uint8_t team_count;
    uint8_t active_index;
    pk_mon_t team[PK_TEAM_MAX];
    uint8_t dex[PK_DEX_BYTES];
    uint8_t shiny_dex[PK_DEX_BYTES];
    uint32_t shards;
    uint32_t wins;
    uint32_t battles;
    uint16_t balls[PK_BALL_COUNT];
    uint32_t recent_game_ids[PK_RECENT_GAMES];
    uint8_t recent_next;
} pk_profile_t;
typedef struct {
    uint16_t xp_awarded;
    uint8_t levels_gained;
    uint8_t new_pokeballs;
    uint8_t new_ultraballs;
    uint8_t new_masterballs;
} pk_reward_t;
void pk_profile_init(pk_profile_t *p);
bool pk_register_discovery(pk_profile_t *p, uint16_t species, bool shiny);
bool pk_dex_has(const pk_profile_t *p, uint16_t species, bool shiny);
uint16_t pk_dex_count(const pk_profile_t *p);
bool pk_team_add(pk_profile_t *p, uint16_t species, bool shiny);
/* Apply only a confirmed unique game. ID 0 is invalid; duplicate IDs in
 * the recent ring are rejected. Never call from a live/unconfirmed game.
 * Returns false if invalid or already applied. */
bool pk_award_game(pk_profile_t *p, uint32_t game_id, bool won, pk_reward_t *reward);
#ifdef __cplusplus
}
#endif
#endif
