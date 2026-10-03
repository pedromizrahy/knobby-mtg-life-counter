#include "timer.h"
#include "game_event.h"
#include "storage.h"
#include "game.h"
#include "damage_log.h"

// Forward declaration
extern void refresh_turn_ui(void);
extern void refresh_player_ui(void);
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
int turn_in_round = 0;
bool turn_reminder_active = false;
bool turn_reminder_flash_on = false;
bool turn_reminder_overlay_active = false;
uint8_t turn_reminder_pulse_level = 0;
bool turn_hold_active = false;
int turn_hold_progress = 0;

#define TURN_HOLD_MS 1000U
#define TURN_REMINDER_ALERT_PERIOD_MS 250U
#define TURN_REMINDER_PULSE_PERIOD_MS 50U

static uint32_t current_turn_started_ms = 0;
static uint32_t turn_hold_started_ms = 0;
static bool turn_hold_completed = false;
static uint8_t turn_blink_steps_remaining = 0;
static uint8_t turn_reminder_flash_steps_remaining = 0;
static int8_t turn_reminder_pulse_dir = 1;

static lv_timer_t *turn_timer = NULL;
static lv_timer_t *turn_blink_timer = NULL;
static lv_timer_t *turn_hold_timer = NULL;
static lv_timer_t *turn_reminder_flash_timer = NULL;

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

void turn_timer_start_for_player(int player)
{
    int player_count = nvs_get_players_to_track();

    if (player_count < 1) player_count = 1;
    if (player < 0 || player >= player_count) player = 0;

    turn_number = 1;
    round_number = 1;
    turn_in_round = 1;
    active_turn_player = player;
    turn_elapsed_ms = 0;
    turn_started_ms = lv_tick_get();
    current_turn_started_ms = turn_started_ms;
    turn_timer_enabled = true;
    turn_indicator_visible = true;
    turn_ui_visible = true;
    /* Solo mode should start stable; the multiplayer start blink is useful
       as a turn-owner cue but looks like a broken timer on a personal Dial. */
    turn_blink_steps_remaining = (player_count > 1) ? 10 : 0;
    turn_reminder_active = false;
    turn_reminder_flash_on = false;
    turn_reminder_overlay_active = false;
    turn_reminder_pulse_level = 0;
    turn_reminder_pulse_dir = 1;
    turn_reminder_flash_steps_remaining = 0;
    turn_hold_active = false;
    turn_hold_progress = 0;
    turn_hold_completed = false;

    if (turn_reminder_flash_timer != NULL) {
        lv_timer_set_period(turn_reminder_flash_timer, TURN_REMINDER_ALERT_PERIOD_MS);
        lv_timer_pause(turn_reminder_flash_timer);
    }

    game_event_add_turn(GAME_EVENT_TURN_START, active_turn_player,
                        (uint16_t)turn_number, (uint16_t)round_number, 0);

    if (turn_blink_timer != NULL) {
        if (turn_blink_steps_remaining > 0)
            lv_timer_resume(turn_blink_timer);
        else
            lv_timer_pause(turn_blink_timer);
    }

    refresh_player_ui();
}

void turn_timer_start_fresh(void)
{
    turn_timer_start_for_player(0);
}

void turn_timer_reset(void)
{
    turn_timer_enabled = false;
    turn_elapsed_ms = 0;
    turn_started_ms = 0;
    current_turn_started_ms = 0;
    turn_number = 0;
    round_number = 0;
    turn_in_round = 0;
    active_turn_player = -1;
    turn_indicator_visible = true;
    turn_ui_visible = false;
    turn_blink_steps_remaining = 0;
    turn_reminder_active = false;
    turn_reminder_flash_on = false;
    turn_reminder_overlay_active = false;
    turn_reminder_pulse_level = 0;
    turn_reminder_pulse_dir = 1;
    turn_reminder_flash_steps_remaining = 0;
    turn_hold_active = false;
    turn_hold_progress = 0;
    turn_hold_completed = false;

    if (turn_blink_timer != NULL) {
        lv_timer_pause(turn_blink_timer);
    }
    if (turn_hold_timer != NULL) {
        lv_timer_pause(turn_hold_timer);
    }
    if (turn_reminder_flash_timer != NULL) {
        lv_timer_set_period(turn_reminder_flash_timer, TURN_REMINDER_ALERT_PERIOD_MS);
        lv_timer_pause(turn_reminder_flash_timer);
    }

    refresh_player_ui();
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
    damage_log_add_turn_end(active_turn_player, round_number, turn_in_round,
                            duration_ms);

    /* Preserve the original total-game timer semantics. */
    turn_elapsed_ms = get_turn_elapsed_ms();
    turn_started_ms = lv_tick_get();

    player_count = nvs_get_players_to_track();
    if (player_count < 1) player_count = 1;

    {
        int previous_player = active_turn_player;
        int attempts = 0;
        int active_players = 0;
        int i;

        for (i = 0; i < player_count; i++) {
            if (!player_eliminated[i]) active_players++;
        }
        if (active_players < 1) active_players = 1;

        do {
            active_turn_player++;
            if (active_turn_player >= player_count) {
                active_turn_player = 0;
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

        turn_in_round++;
        if (turn_in_round > active_players) {
            round_number++;
            turn_in_round = 1;
        }
    }

    turn_number++;
    current_turn_started_ms = lv_tick_get();
    turn_reminder_active = false;
    turn_reminder_flash_on = false;
    turn_reminder_overlay_active = false;
    turn_reminder_pulse_level = 0;
    turn_reminder_pulse_dir = 1;
    turn_reminder_flash_steps_remaining = 0;
    turn_hold_active = false;
    turn_hold_progress = 0;
    turn_hold_completed = false;

    if (turn_reminder_flash_timer != NULL) {
        lv_timer_set_period(turn_reminder_flash_timer, TURN_REMINDER_ALERT_PERIOD_MS);
        lv_timer_pause(turn_reminder_flash_timer);
    }

    game_event_add_turn(GAME_EVENT_TURN_START, active_turn_player,
                        (uint16_t)turn_number, (uint16_t)round_number, 0);

    /* A life-selection highlight must not outlive a turn change, otherwise
       it visually overrides the new active player. */
    selection_clear();

    /* Turn ownership affects panel vibrancy, so refresh the player UI when
       the active seat changes. Timer ticks still use the lightweight
       refresh_turn_ui() path to avoid redrawing every panel each second. */
    refresh_player_ui();
}

void turn_reminder_reconcile_after_setting_change(void)
{
    int reminder_minutes = nvs_get_turn_reminder_minutes();
    uint32_t reminder_ms =
        (uint32_t)(reminder_minutes > 0 ? reminder_minutes : 0) * 60U * 1000U;
    bool crossed =
        turn_timer_enabled &&
        active_turn_player >= 0 &&
        reminder_minutes > 0 &&
        get_current_turn_elapsed_ms() >= reminder_ms;

    /*
     * Changing the threshold is configuration, not a new threshold-crossing
     * event. If the new value is already behind the running turn, enter the
     * persistent breathing state directly and skip the big MIN overlay.
     */
    if (crossed) {
        turn_reminder_active = true;
        turn_reminder_flash_on = true;
        turn_reminder_overlay_active = false;
        turn_reminder_flash_steps_remaining = 0;
        if (turn_reminder_pulse_level < 24)
            turn_reminder_pulse_level = 30;
        turn_reminder_pulse_dir = 1;

        if (turn_reminder_flash_timer != NULL) {
            lv_timer_set_period(turn_reminder_flash_timer,
                                TURN_REMINDER_PULSE_PERIOD_MS);
            lv_timer_reset(turn_reminder_flash_timer);
            if (nvs_get_turn_visual_alert())
                lv_timer_resume(turn_reminder_flash_timer);
            else
                lv_timer_pause(turn_reminder_flash_timer);
        }
    } else {
        turn_reminder_active = false;
        turn_reminder_flash_on = false;
        turn_reminder_overlay_active = false;
        turn_reminder_flash_steps_remaining = 0;
        turn_reminder_pulse_level = 0;
        turn_reminder_pulse_dir = 1;

        if (turn_reminder_flash_timer != NULL) {
            lv_timer_set_period(turn_reminder_flash_timer,
                                TURN_REMINDER_ALERT_PERIOD_MS);
            lv_timer_pause(turn_reminder_flash_timer);
        }
    }

    refresh_turn_ui();
}

// ---------- timer callbacks ----------
static void turn_timer_tick_cb(lv_timer_t *timer)
{
    uint32_t reminder_ms;
    int reminder_minutes;
    bool reminder_crossed;
    (void)timer;

    reminder_minutes = nvs_get_turn_reminder_minutes();
    reminder_ms = (uint32_t)reminder_minutes * 60U * 1000U;
    reminder_crossed =
        turn_timer_enabled &&
        reminder_minutes > 0 &&
        get_current_turn_elapsed_ms() >= reminder_ms;

    if (reminder_crossed && !turn_reminder_active) {
        turn_reminder_active = true;
        if (nvs_get_turn_visual_alert()) {
            int alert_seconds = nvs_get_turn_alert_duration_seconds();
            turn_reminder_flash_steps_remaining =
                (uint8_t)((alert_seconds * 1000U) / TURN_REMINDER_ALERT_PERIOD_MS);
            if (turn_reminder_flash_steps_remaining == 0)
                turn_reminder_flash_steps_remaining = 1;
            turn_reminder_flash_on = true;
            turn_reminder_overlay_active = true;
            turn_reminder_pulse_level = 30;
            turn_reminder_pulse_dir = 1;
            if (turn_reminder_flash_timer != NULL) {
                lv_timer_set_period(turn_reminder_flash_timer, TURN_REMINDER_ALERT_PERIOD_MS);
                lv_timer_reset(turn_reminder_flash_timer);
                lv_timer_resume(turn_reminder_flash_timer);
            }
        }
    } else if (!reminder_crossed) {
        turn_reminder_active = false;
        turn_reminder_flash_on = false;
        turn_reminder_overlay_active = false;
        turn_reminder_flash_steps_remaining = 0;
        if (turn_reminder_flash_timer != NULL) {
            lv_timer_pause(turn_reminder_flash_timer);
        }
    }

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

static void turn_hold_timer_cb(lv_timer_t *timer)
{
    uint32_t elapsed;
    (void)timer;

    if (!turn_hold_active) {
        lv_timer_pause(turn_hold_timer);
        return;
    }

    elapsed = lv_tick_elaps(turn_hold_started_ms);
    if (elapsed >= TURN_HOLD_MS) {
        turn_hold_progress = 1000;
        refresh_turn_ui();

        turn_hold_active = false;
        turn_hold_completed = true;
        lv_timer_pause(turn_hold_timer);
        turn_advance();
        return;
    }

    turn_hold_progress = (int)((elapsed * 1000U) / TURN_HOLD_MS);
    refresh_turn_ui();
}

static void turn_reminder_flash_timer_cb(lv_timer_t *timer)
{
    if (!turn_reminder_active || !nvs_get_turn_visual_alert()) {
        turn_reminder_flash_on = false;
        turn_reminder_overlay_active = false;
        turn_reminder_pulse_level = 0;
        turn_reminder_pulse_dir = 1;
        lv_timer_set_period(timer, TURN_REMINDER_ALERT_PERIOD_MS);
        lv_timer_pause(timer);
        refresh_turn_ui();
        return;
    }

    if (turn_reminder_overlay_active) {
        if (turn_reminder_flash_steps_remaining > 0)
            turn_reminder_flash_steps_remaining--;

        if (turn_reminder_flash_steps_remaining == 0) {
            turn_reminder_overlay_active = false;
            turn_reminder_flash_on = true;
            turn_reminder_pulse_level = 30;
            turn_reminder_pulse_dir = 1;
            lv_timer_set_period(timer, TURN_REMINDER_PULSE_PERIOD_MS);
            lv_timer_reset(timer);
        }

        refresh_turn_ui();
        return;
    }

    /* Smooth breathing halo after the threshold alert. */
    if (turn_reminder_pulse_dir > 0) {
        if (turn_reminder_pulse_level >= 100) {
            turn_reminder_pulse_level = 100;
            turn_reminder_pulse_dir = -1;
        } else {
            turn_reminder_pulse_level = (uint8_t)(turn_reminder_pulse_level + 2);
        }
    } else {
        if (turn_reminder_pulse_level <= 20) {
            turn_reminder_pulse_level = 20;
            turn_reminder_pulse_dir = 1;
        } else {
            turn_reminder_pulse_level = (uint8_t)(turn_reminder_pulse_level - 2);
        }
    }

    turn_reminder_flash_on = true;
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
    /* Kept for compatibility with older screens. Turn passing now uses
       event_turn_hold() so a stray tap cannot advance game history. */
}

void event_turn_hold(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        if (!turn_timer_enabled || active_turn_player < 0) return;

        /* A life preview can still be waiting on its auto-commit timer.
           Resolve it synchronously before arming turn advance so the preview
           callback and turn/log mutation can never race each other. */
        if (life_preview_active) {
            life_preview_commit_cb(NULL);
        }

        turn_hold_started_ms = lv_tick_get();
        turn_hold_active = true;
        turn_hold_completed = false;
        turn_hold_progress = 0;

        if (turn_hold_timer != NULL) {
            lv_timer_reset(turn_hold_timer);
            lv_timer_resume(turn_hold_timer);
        }
        refresh_turn_ui();
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!turn_hold_completed) {
            turn_hold_active = false;
            turn_hold_progress = 0;
            if (turn_hold_timer != NULL) {
                lv_timer_pause(turn_hold_timer);
            }
            refresh_turn_ui();
        } else {
            turn_hold_completed = false;
        }
    }
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

    turn_hold_timer = lv_timer_create(turn_hold_timer_cb, 50, NULL);
    if (turn_hold_timer != NULL) {
        lv_timer_pause(turn_hold_timer);
    }

    turn_reminder_flash_timer = lv_timer_create(
        turn_reminder_flash_timer_cb, TURN_REMINDER_ALERT_PERIOD_MS, NULL);
    if (turn_reminder_flash_timer != NULL) {
        lv_timer_pause(turn_reminder_flash_timer);
    }
}
