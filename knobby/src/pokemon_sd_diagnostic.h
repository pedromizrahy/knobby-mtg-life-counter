#ifndef POKEMON_SD_DIAGNOSTIC_H
#define POKEMON_SD_DIAGNOSTIC_H
#ifdef __cplusplus
extern "C" {
#endif
/* Explicit manual serial test, mounts microSD without formatting and never
 * writes, creates, renames or deletes any file. No gameplay call sites. */
void pk_sd_status_read_only(void);
/* Bounded directory inventory (max 100 entries, depth 2), read-only. */
void pk_sd_list_read_only(void);
/* Explicit, allowlisted factory-demo cleanup; preview is read-only. */
void pk_sd_cleanup_preview(void);
void pk_sd_cleanup_confirm(void);
#ifdef __cplusplus
}
#endif
#endif
