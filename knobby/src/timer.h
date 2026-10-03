#ifndef _TIMER_H
#define _TIMER_H

#include "types.h"

// ---------- state ----------
extern bool turn_timer_enabled;
extern bool turn_indicator_visible;
extern bool turn_ui_visible;
extern uint32_t turn_elapsed_ms;
extern uint32_t turn_started_ms;
extern int turn_number;

/* Structured turn tracking. These are intentionally separate from the
   existing total-game timer so current UI behavior stays compatible while
   richer per-player timing is added incrementally. */
extern int active_turn_player;
extern int round_number;
extern int turn_in_round;
extern bool turn_reminder_active;
extern bool turn_reminder_flash_on;
extern bool turn_reminder_overlay_active;
extern uint8_t turn_reminder_pulse_level;
extern bool turn_hold_active;
extern int turn_hold_progress;

// ---------- functions ----------
void knob_timer_init(void);
void turn_timer_start_fresh(void);
void turn_timer_start_for_player(int player);
void turn_timer_reset(void);
uint32_t get_turn_elapsed_ms(void);
uint32_t get_current_turn_elapsed_ms(void);
void turn_advance(void);

// event callbacks used in screen builders
void event_tool_timer(lv_event_t *e);
void event_turn_tap(lv_event_t *e);
void event_turn_hold(lv_event_t *e);

#endif // _TIMER_H
