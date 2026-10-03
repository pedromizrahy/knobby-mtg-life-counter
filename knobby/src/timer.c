#include "timer.h"
#include "game_event.h"
#include "storage.h"
#include "game.h"

// Forward declaration
extern void refresh_turn_ui(void);
extern void back_to_main(void);

// ---------- state ----------
bool turn_timer_enabled = false;
bool turn_indicator_visible = true;
bool turn_ui_visible = false;
uint32_t turn_elapsed_ms = 0;
uint32_t turn_started_ms = 0;
int turn_number = 0;

int active_turn_player = -1;
int round_number = 0;

static uint32_t current_turn_started_ms = 0;
static uint8_t turn_blink_steps_remaining = 0;

static lv_timer_t *turn_timer = NULL;
static lv_timer_t *turn_blink_timer = NULL;

// ---------- functions ----------
uint32_t get_turn_elapsed_ms(void)
{
    uint32_t elapsed = turn_elapsed_ms;

    if (turn_timer_enabled) {
        elapsed += lv_tick_elaps(turn_started_ms);
    }

    return elapsed;
}

uint32_t get_current_turn_elapsed_ms(void)
{
    if (!turn_timer_enabled || active_turn_player < 0) return 0;
    return lv_tick_elaps(current_turn_started_ms);
}

void turn_timer_start_fresh(void)
{
    turn_number = 1;
    round_number = 1;
    active_turn_player = 0;
    turn_elapsed_ms = 0;
    turn_started_ms = lv_tick_get();
    current_turn_started_ms = turn_started_ms;
    turn_timer_enabled = true;
    turn_indicator_visible = true;
    turn_ui_visible = true;
    turn_blink_steps_remaining = 10;

    game_event_add_turn(GAME_EVENT_TURN_START, active_turn_player,
                        (uint16_t)turn_number, (uint16_t)round_number, 0);

    if (turn_blink_timer != NULL) {
        lv_timer_resume(turn_blink_timer);
    }

    refresh_turn_ui();
}

void turn_timer_reset(void)
{
    turn_timer_enabled = false;
    turn_elapsed_ms = 0;
    turn_started_ms = 0;
    current_turn_started_ms = 0;
    turn_number = 0;
    round_number = 0;
    active_turn_player = -1;
    turn_indicator_visible = true;
    turn_ui_visible = false;
    turn_blink_steps_remaining = 0;

    if (turn_blink_timer != NULL) {
        lv_timer_pause(turn_blink_timer);
    }

    refresh_turn_ui();
}

void turn_advance(void)
{
    int player_count;
    uint32_t duration_ms;

    if (!turn_timer_enabled || active_turn_player < 0) {
        turn_timer_start_fresh();
        return;
    }

    duration_ms = get_current_turn_elapsed_ms();
    game_event_add_turn(GAME_EVENT_TURN_END, active_turn_player,
                        (uint16_t)turn_number, (uint16_t)round_number,
                        duration_ms);

    /* Preserve the original total-game timer semantics. */
    turn_elapsed_ms = get_turn_elapsed_ms();
    turn_started_ms = lv_tick_get();

    player_count = nvs_get_players_to_track();
    if (player_count < 1) player_count = 1;

    {
        int previous_player = active_turn_player;
        int attempts = 0;

        do {
            active_turn_player++;
            if (active_turn_player >= player_count) {
                active_turn_player = 0;
                round_number++;
            }
            attempts++;
        } while (attempts < player_count &&
                 player_eliminated[active_turn_player]);

        /* If every tracked opponent is eliminated, keep the current seat
           instead of spinning forever. Game-end/winner handling will own
           this state in a later milestone. */
        if (attempts >= player_count && player_eliminated[active_turn_player]) {
            active_turn_player = previous_player;
        }
    }

    turn_number++;
    current_turn_started_ms = lv_tick_get();

    game_event_add_turn(GAME_EVENT_TURN_START, active_turn_player,
                        (uint16_t)turn_number, (uint16_t)round_number, 0);

    refresh_turn_ui();
}

// ---------- timer callbacks ----------
static void turn_timer_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_turn_ui();
}

static void turn_blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (turn_blink_steps_remaining == 0) {
        turn_indicator_visible = true;
        if (turn_blink_timer != NULL) {
            lv_timer_pause(turn_blink_timer);
        }
        refresh_turn_ui();
        return;
    }

    turn_indicator_visible = !turn_indicator_visible;
    turn_blink_steps_remaining--;
    refresh_turn_ui();
}

// ---------- event callbacks ----------
void event_tool_timer(lv_event_t *e)
{
    (void)e;
    turn_timer_start_fresh();
    back_to_main();
}

void event_turn_tap(lv_event_t *e)
{
    (void)e;
    turn_advance();
}

// ---------- init ----------
void knob_timer_init(void)
{
    turn_timer = lv_timer_create(turn_timer_tick_cb, 1000, NULL);
    if (turn_timer != NULL) {
        lv_timer_ready(turn_timer);
    }

    turn_blink_timer = lv_timer_create(turn_blink_timer_cb, 500, NULL);
    if (turn_blink_timer != NULL) {
        lv_timer_pause(turn_blink_timer);
    }
}
