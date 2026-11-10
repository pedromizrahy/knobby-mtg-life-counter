#include "game_event.h"

static game_event_t game_events[GAME_EVENT_MAX];
static int game_event_head = 0;
static int game_event_size = 0;

void game_event_reset(void)
{
    game_event_head = 0;
    game_event_size = 0;
}

bool game_event_add(const game_event_t *event)
{
    game_event_t copy;

    if (event == NULL) return false;

    copy = *event;
    if (copy.timestamp_ms == 0) {
        copy.timestamp_ms = lv_tick_get();
    }

    game_events[game_event_head] = copy;
    game_event_head = (game_event_head + 1) % GAME_EVENT_MAX;
    if (game_event_size < GAME_EVENT_MAX) {
        game_event_size++;
    }

    return true;
}

int game_event_count(void)
{
    return game_event_size;
}

bool game_event_get(int newest_offset, game_event_t *out)
{
    int index;

    if (out == NULL) return false;
    if (newest_offset < 0 || newest_offset >= game_event_size) return false;

    index = (game_event_head - 1 - newest_offset + GAME_EVENT_MAX) % GAME_EVENT_MAX;
    *out = game_events[index];
    return true;
}

void game_event_add_turn(uint8_t type, int player, uint16_t turn_number,
                         uint16_t round_number, uint32_t duration_ms)
{
    game_event_t event = {0};

    event.type = type;
    event.damage_type = DAMAGE_TYPE_NONE;
    event.source_player = (int8_t)player;
    event.target_player = GAME_EVENT_NO_PLAYER;
    event.duration_ms = duration_ms;
    event.turn_number = turn_number;
    event.round_number = round_number;

    game_event_add(&event);
}
