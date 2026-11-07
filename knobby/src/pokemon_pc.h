#ifndef POKEMON_PC_H
#define POKEMON_PC_H
#include <stddef.h>
#include <stdbool.h>
#include "pokemon_progress.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Pure model for a six-member team and an independently owned PC.
 * The caller provides an appropriately sized array and authenticates the
 * Playgroup user. NO filesystem writes, rewards, ID inference or Gacha. */
typedef struct {
    pk_mon_t *entries;
    size_t count;
    size_t capacity;
} pk_pc_t;
/* The main Pokemon must already occupy a team slot. */
bool pk_pc_set_main(pk_profile_t *profile,unsigned team_slot);
/* Accept only a capture already validated/committed by the game service.
 * Duplicate species, normal/shiny, each retain independent LV/XP. */
bool pk_pc_add_captured(pk_pc_t *pc,pk_mon_t mon);
/* PC swaps preserve owned instances (species, shiny, level and XP). */
bool pk_pc_swap_team(pk_profile_t *profile,pk_pc_t *pc,
                     unsigned team_slot,size_t pc_slot);
bool pk_pc_take_team(pk_profile_t *profile,pk_pc_t *pc,size_t pc_slot);
/* Cannot deposit the main, or leave the team without a main. */
bool pk_pc_deposit_team(pk_profile_t *profile,pk_pc_t *pc,unsigned team_slot);
#ifdef __cplusplus
}
#endif
#endif
