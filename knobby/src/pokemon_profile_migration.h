#ifndef POKEMON_PROFILE_MIGRATION_H
#define POKEMON_PROFILE_MIGRATION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    PK_MIGRATE_ERROR=-2,
    PK_MIGRATE_CONFLICT=-1,
    PK_MIGRATE_NOTHING=0,
    PK_MIGRATE_COPIED=1,
    PK_MIGRATE_ALREADY=2,
    PK_MIGRATE_FILE_ONLY=3
} pk_migration_result_t;
/* Explicit operator-initiated migration. NEVER auto-called at boot,
 * game completion or sync; does not erase or mutate the source NVS data.
 * Caller must serialize all access to this user's profile during migration.
 */
pk_migration_result_t pk_profile_migrate_to_spiffs(uint32_t user_id);
#ifdef __cplusplus
}
#endif
#endif
