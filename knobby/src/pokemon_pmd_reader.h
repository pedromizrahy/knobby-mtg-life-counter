#ifndef POKEMON_PMD_READER_H
#define POKEMON_PMD_READER_H

#ifdef __cplusplus
extern "C" {
#endif
/* Manual, read-only validation of a TamaPoke PMD TPK2 animation container.
 * Reads /mons/p778.bin (Mimikyu #778) over K518/K718 SDMMC 1-bit.
 * No file modifications, no implicit formatting, no Magic gameplay use.
 */
void pk_pmd_check_mimikyu_read_only(void);
/* Generic post-transfer read-only check, ID 1..1025. */
void pk_pmd_check_species_read_only(unsigned species);
/* Explicit staged serial transfer. No sprite files are overwritten. */
void pk_pmd_put_species(unsigned species,unsigned long size,unsigned long expected_crc);
void pk_pmd_put_mimikyu(unsigned long size, unsigned long expected_crc);
#ifdef __cplusplus
}
#endif
#endif
