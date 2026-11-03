#ifndef POKEMON_SD_DIAGNOSTIC_H
#define POKEMON_SD_DIAGNOSTIC_H
#ifdef __cplusplus
extern "C" {
#endif
/* Explicit manual serial test, mounts microSD without formatting and never
 * writes, creates, renames or deletes any file. No gameplay call sites. */
void pk_sd_status_read_only(void);
#ifdef __cplusplus
}
#endif
#endif
