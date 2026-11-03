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
/* Explicit hardware-only diagnostic: reserved isolated profile, verifies
 * alternating copies and recovery, cleans only files it creates. */
bool pk_file_profile_selftest(void);
/* Two-stage manual power-cycle test using a DIFFERENT reserved ID.
 * PREP writes two verified slots, VERIFY confirms their expected bytes and
 * cleans them. Reboot the dial between commands; firmware does not reboot
 * the device automatically or touch real players. */
bool pk_file_reboot_prep(void);
bool pk_file_reboot_verify(void);
/* Copy an existing validated NVS profile into an EMPTY SPIFFS profile.
 * Uses a unique temporary file and rename; does not alter NVS or existing
 * SPIFFS profiles. Explicit opt-in: NOT invoked automatically by firmware. */
bool pk_file_import_if_missing(uint32_t user_id,const pk_profile_t *profile);
#ifdef __cplusplus
}
#endif
#endif
