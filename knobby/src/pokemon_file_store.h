#ifndef POKEMON_FILE_STORE_H
#define POKEMON_FILE_STORE_H
#include <stdint.h>
#include <stdbool.h>
#include "pokemon_profile_store.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Isolated SPIFFS backend prototype. No Magic/gameplay call sites and no
 * migration from NVS yet. Read-only mount: NEVER formats SPIFFS.
 * Two independently validated file slots per Playgroup user ID.
 */
pk_store_status_t pk_file_inspect(uint32_t user_id,pk_profile_t *out);
bool pk_file_create_starter(uint32_t user_id);
bool pk_file_save(uint32_t user_id,const pk_profile_t *profile);
#ifdef __cplusplus
}
#endif
#endif
