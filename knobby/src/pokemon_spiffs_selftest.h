#ifndef POKEMON_SPIFFS_SELFTEST_H
#define POKEMON_SPIFFS_SELFTEST_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Manual-only destructive test of ONE newly created diagnostic file.
 * Refuses to run if its reserved path already exists; never formats or
 * touches real profile, ledger, queue, migration, or art files.
 */
bool pk_spiffs_selftest(void);
#ifdef __cplusplus
}
#endif
#endif
