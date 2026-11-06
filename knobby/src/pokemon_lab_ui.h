#ifndef POKEMON_LAB_UI_H
#define POKEMON_LAB_UI_H
#ifdef __cplusplus
extern "C" {
#endif
/* Built together with the companion screen; opened explicitly from the
 * STATS panel. RAM-only simulator, never a Playgroup/real reward screen. */
void pokemon_lab_ui_build(void);
void pokemon_lab_ui_open(void);
#ifdef __cplusplus
}
#endif
#endif
