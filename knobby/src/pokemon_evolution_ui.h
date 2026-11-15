#ifndef POKEMON_EVOLUTION_UI_H
#define POKEMON_EVOLUTION_UI_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Opt-in, READ-ONLY EVOLVE preview for a linked, freshly authenticated
 * Playgroup member. Never changes, claims or consumes any item/profile. */
bool pk_evolution_ui_open(uint32_t expected_user_id);
#ifdef __cplusplus
}
#endif
#endif
