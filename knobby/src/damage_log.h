#ifndef _DAMAGE_LOG_H
#define _DAMAGE_LOG_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAMAGE_LOG_MAX 256

typedef enum {
    LOG_EVT_LIFE = 0,
    LOG_EVT_DAMAGE,
    LOG_EVT_CMD_DAMAGE,
    LOG_EVT_CMD_INFECT,
    LOG_EVT_POISON,
    LOG_EVT_COUNTER,
    LOG_EVT_TURN_END,
} log_event_type_t;

extern lv_obj_t *screen_damage_log;

typedef struct {
    uint32_t timestamp_ms;
    int8_t player;
    int8_t source;
    uint8_t event_type;
    int16_t delta;
    uint16_t turn_number;
    uint16_t round_number;
    uint16_t turn_in_round;
    int8_t turn_player;
    uint32_t duration_ms;
    uint32_t turn_elapsed_ms;
    uint16_t action_id;
} damage_log_record_t;

void damage_log_add(int player, int delta, uint8_t event_type, int source);
void damage_log_add_turn_end(int player, int round, int turn_in_round,
                             uint32_t duration_ms);
void damage_log_begin_action(void);
void damage_log_end_action(void);
void damage_log_reset(void);
int damage_log_record_count(void);
bool damage_log_record_get_oldest(int oldest_index, damage_log_record_t *out);
void damage_log_remove_last_for(int player, uint8_t event_type);
void damage_log_select_next(void);
void damage_log_select_prev(void);
void damage_log_undo_selected(void);
void damage_log_undo_all(void);

void build_damage_log_screen(void);
void open_damage_log_screen(void);

#ifdef __cplusplus
}
#endif

#endif // _DAMAGE_LOG_H
