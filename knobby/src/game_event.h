#ifndef _GAME_EVENT_H
#define _GAME_EVENT_H

#include "types.h"

#define GAME_EVENT_MAX 256
#define GAME_EVENT_NO_PLAYER (-1)

typedef enum {
    GAME_EVENT_LIFE_CHANGE = 0,
    GAME_EVENT_DAMAGE,
    GAME_EVENT_COMMANDER_DAMAGE,
    GAME_EVENT_COUNTER_CHANGE,
    GAME_EVENT_TURN_START,
    GAME_EVENT_TURN_END,
    GAME_EVENT_MONARCH_CHANGE,
    GAME_EVENT_INITIATIVE_CHANGE,
    GAME_EVENT_DAY_NIGHT_CHANGE,
    GAME_EVENT_ELIMINATION,
    GAME_EVENT_GAME_END,
} game_event_type_t;

typedef enum {
    DAMAGE_TYPE_NONE = 0,
    DAMAGE_TYPE_NORMAL,
    DAMAGE_TYPE_COMMANDER,
    DAMAGE_TYPE_POISON,
} game_damage_type_t;

typedef struct {
    uint32_t timestamp_ms;
    uint32_t duration_ms;
    uint8_t type;
    uint8_t damage_type;
    int8_t source_player;
    int8_t target_player;
    uint8_t target_mask;
    int16_t amount;
    int16_t value;
    uint16_t turn_number;
    uint16_t round_number;
} game_event_t;

void game_event_reset(void);
bool game_event_add(const game_event_t *event);
int game_event_count(void);
bool game_event_get(int newest_offset, game_event_t *out);

void game_event_add_turn(uint8_t type, int player, uint16_t turn_number,
                         uint16_t round_number, uint32_t duration_ms);

#endif // _GAME_EVENT_H
