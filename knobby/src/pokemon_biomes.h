#ifndef POKEMON_BIOMES_H
#define POKEMON_BIOMES_H
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { PK_BIOME_MEADOW=0, PK_BIOME_FOREST, PK_BIOME_SEA,
       PK_BIOME_VOLCANO, PK_BIOME_CAVE, PK_BIOME_HAUNTED, PK_BIOME_COUNT };
/* Original 180x180 pixel-art backdrops, x2 nearest-neighbor on a 360x360
 * screen. Generated in RAM on selection; no external artwork or SD writes. */
bool pk_biome_render(unsigned biome);
const lv_img_dsc_t *pk_biome_image(void);
void pk_biome_release(void);
#ifdef __cplusplus
}
#endif
#endif
