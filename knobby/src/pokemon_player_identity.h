#ifndef POKEMON_PLAYER_IDENTITY_H
#define POKEMON_PLAYER_IDENTITY_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* The Pokémon namespace currently supports only nonreserved 32-bit
 * Playgroup global numeric user IDs. Seat numbers, names, session IDs,
 * local guests and reserved selftest IDs MUST NOT be used as player IDs.
 * This validates representation only, NOT authentication or ownership. */
bool pk_player_id_valid(uint64_t user_id);
/* Explicitly match a verified external user ID against a COMPLETE set
 * of distinct, mapped Playgroup participants. No seat inference.
 * Returns false on duplicates, guests, reserved ids and absent user.
 * This does NOT authenticate the human in possession of the DIAL. */
bool pk_player_id_in_roster(uint64_t requested_id,
                            const uint64_t *roster,unsigned count);
#ifdef __cplusplus
}
#endif
#endif
