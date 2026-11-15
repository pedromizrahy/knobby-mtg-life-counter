#include "pokemon_profile_migration.h"
#include "pokemon_profile_store.h"
#include "pokemon_file_store.h"
#include <string.h>

extern "C" pk_migration_result_t pk_profile_migrate_to_spiffs(uint32_t user_id)
{
    if(!user_id || user_id==0xFFFFFFFEUL) return PK_MIGRATE_ERROR;
    pk_profile_t legacy={},on_file={};
    const pk_store_status_t source=pk_store_inspect(user_id,&legacy);
    const pk_store_status_t target=pk_file_inspect(user_id,&on_file);
    /* Never interpret unreadable or unavailable data as an empty profile. */
    if(source==PK_STORE_ERROR || target==PK_STORE_ERROR)
        return PK_MIGRATE_ERROR;
    if(source==PK_STORE_CORRUPT || target==PK_STORE_CORRUPT)
        return PK_MIGRATE_CONFLICT;

    if(source==PK_STORE_MISSING) {
        return target==PK_STORE_VALID ? PK_MIGRATE_FILE_ONLY
                                      : PK_MIGRATE_NOTHING;
    }
    if(target==PK_STORE_VALID) {
        return memcmp(&legacy,&on_file,sizeof(legacy))==0
               ? PK_MIGRATE_ALREADY : PK_MIGRATE_CONFLICT;
    }
    if(target!=PK_STORE_MISSING) return PK_MIGRATE_CONFLICT;
    if(!pk_file_import_if_missing(user_id,&legacy))
        return PK_MIGRATE_ERROR;
    pk_profile_t verified={};
    if(pk_file_inspect(user_id,&verified)!=PK_STORE_VALID ||
       memcmp(&legacy,&verified,sizeof(legacy))!=0)
        return PK_MIGRATE_ERROR;
    return PK_MIGRATE_COPIED;
}
