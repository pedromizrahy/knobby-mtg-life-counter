#ifndef POKEMON_LAB_H
#define POKEMON_LAB_H
#include <stdbool.h>
#include <stdint.h>
#include "pokemon_gacha.h"
#ifdef __cplusplus
extern "C" {
#endif
/* RAM-only diagnostic. NO Playgroup user_id, NO NVS/SPIFFS/SD, no wallet,
 * no sync, no reward transactions. All balls are unlimited by design.
 * Synthetic sample species are not installed sprites or owned Pokemon.
 */
#define PK_LAB_MAX_BATCH 1000U
typedef struct {
    bool active;
    bool master_trainer_badge;
    uint8_t simulated_level;
    uint64_t draws[PK_BALL_COUNT];
    uint64_t tiers[PK_BALL_COUNT][PK_TIER_COUNT];
    uint64_t shinies[PK_BALL_COUNT];
    pk_gacha_result_t last;
    uint8_t last_ball;
    bool has_last;
} pk_lab_t;
void pk_lab_start(pk_lab_t *lab);
void pk_lab_reset(pk_lab_t *lab);
bool pk_lab_award_master_badge(pk_lab_t *lab);
bool pk_lab_set_level(pk_lab_t *lab,uint8_t level);
bool pk_lab_draw(pk_lab_t *lab,uint8_t ball,unsigned count,
                 pk_gacha_rng_t rng,void *rng_ctx);
#ifdef __cplusplus
}
#endif
#endif
