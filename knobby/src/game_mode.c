#include "game_mode.h"
#include "storage.h"
#include "settings.h"
#include "ui_mp.h"
#include "ui_1p.h"
#include "timer.h"

// Forward declaration
extern void back_to_main(void);

// ---------- screens ----------
lv_obj_t *screen_game_mode_menu = NULL;
lv_obj_t *screen_game_setup = NULL;
lv_obj_t *screen_game_player_settings = NULL;
lv_obj_t *screen_custom_life = NULL;

// ---------- game-setup dynamic labels ----------
static lv_obj_t *label_gm_num_players = NULL;
static lv_obj_t *label_gm_players_to_track = NULL;
static lv_obj_t *label_gm_life_total = NULL;

// ---------- player-settings dynamic labels ----------
static lv_obj_t *label_gm_color_mode = NULL;
static lv_obj_t *label_gm_cmd_marker = NULL;
static lv_obj_t *label_gm_orientation = NULL;
static lv_obj_t *label_gm_name_display = NULL;

// ---------- custom life widgets ----------
static lv_obj_t *label_custom_life_value = NULL;

// ---------- temp game settings (committed by Apply) ----------
static int temp_num_players;
static int temp_players_to_track;
static int temp_life_total;

// ---------- refresh ----------
static void refresh_player_settings_ui(void)
{
    if (label_gm_color_mode != NULL) {
        lv_label_set_text(label_gm_color_mode,
            nvs_get_color_mode() == COLOR_MODE_LIFE
                ? "Colors\nLife" : "Colors\nPlayer");
    }

    if (label_gm_cmd_marker != NULL) {
        lv_label_set_text(label_gm_cmd_marker,
            nvs_get_cmd_marker_mode() == CMD_MARKER_ART
                ? "Cmd Marker\nArt" : "Cmd Marker\nDot");
    }

    if (label_gm_orientation != NULL) {
        const char *name = "Absolute";
        if (nvs_get_orientation() == ORIENTATION_MODE_CENTRIC)
            name = "Centric";
        else if (nvs_get_orientation() == ORIENTATION_MODE_TABLETOP)
            name = "Tabletop";

        {
            char buf[32];
            snprintf(buf, sizeof(buf), "Orientation\n%s", name);
            lv_label_set_text(label_gm_orientation, buf);
        }
    }

    if (label_gm_name_display != NULL) {
        lv_label_set_text(label_gm_name_display,
            nvs_get_turn_show_name()
                ? "Name Display\nON" : "Name Display\nOFF");
    }
}

void refresh_game_mode_menu_ui(void)
{
    char buf[32];
    int max_track;

    if (label_gm_num_players != NULL) {
        snprintf(buf, sizeof(buf), "Players\n%d", temp_num_players);
        lv_label_set_text(label_gm_num_players, buf);
    }

    max_track = temp_num_players < MAX_DISPLAY_PLAYERS
              ? temp_num_players : MAX_DISPLAY_PLAYERS;
    if (temp_players_to_track > max_track)
        temp_players_to_track = max_track;

    if (label_gm_players_to_track != NULL) {
        snprintf(buf, sizeof(buf), "Track\n%d", temp_players_to_track);
        lv_label_set_text(label_gm_players_to_track, buf);
    }

    if (label_gm_life_total != NULL) {
        snprintf(buf, sizeof(buf), "Life\n%d", temp_life_total);
        lv_label_set_text(label_gm_life_total, buf);
    }

    refresh_player_settings_ui();
}

void refresh_custom_life_ui(void)
{
    char buf[16];

    if (label_custom_life_value == NULL) return;
    snprintf(buf, sizeof(buf), "%d", temp_life_total);
    lv_label_set_text(label_custom_life_value, buf);
}

// ---------- navigation ----------
void open_game_mode_menu(void)
{
    temp_num_players = nvs_get_num_players();
    temp_players_to_track = nvs_get_players_to_track();
    temp_life_total = nvs_get_life_total();
    refresh_game_mode_menu_ui();
    lv_scr_load(screen_game_mode_menu);
}

void return_to_game_mode_menu(void)
{
    refresh_game_mode_menu_ui();
    lv_scr_load(screen_game_mode_menu);
}

// ---------- knob input ----------
void change_custom_life(int delta)
{
    temp_life_total += delta;
    if (temp_life_total < 1) temp_life_total = 1;
    if (temp_life_total > LIFE_MAX) temp_life_total = LIFE_MAX;
    refresh_custom_life_ui();
}

// ---------- game-setup events ----------
static void event_gm_open_setup(lv_event_t *e)
{
    (void)e;
    refresh_game_mode_menu_ui();
    lv_scr_load(screen_game_setup);
}

static void event_gm_num_players(lv_event_t *e)
{
    (void)e;

    temp_num_players++;
    if (temp_num_players > MAX_DISPLAY_PLAYERS)
        temp_num_players = 1;

    /* Local play follows the selected table size by default. Track remains
       available as an explicit override on the setup page. */
    temp_players_to_track = temp_num_players;
    refresh_game_mode_menu_ui();
}

static void event_gm_players_to_track(lv_event_t *e)
{
    int max_track;
    (void)e;

    max_track = temp_num_players < MAX_DISPLAY_PLAYERS
              ? temp_num_players : MAX_DISPLAY_PLAYERS;
    temp_players_to_track++;
    if (temp_players_to_track > max_track)
        temp_players_to_track = 1;

    refresh_game_mode_menu_ui();
}

static void event_gm_life_cycle(lv_event_t *e)
{
    (void)e;

    if (temp_life_total == 20) temp_life_total = 25;
    else if (temp_life_total == 25) temp_life_total = 30;
    else if (temp_life_total == 30) temp_life_total = 40;
    else temp_life_total = 20;

    refresh_game_mode_menu_ui();
}

static void event_gm_life_custom(lv_event_t *e)
{
    (void)e;
    refresh_custom_life_ui();
    lv_scr_load(screen_custom_life);
    if (lv_indev_get_act() != NULL)
        lv_indev_wait_release(lv_indev_get_act());
}

static void event_gm_setup_back(lv_event_t *e)
{
    (void)e;
    return_to_game_mode_menu();
}

// ---------- player-settings events ----------
static void event_gm_color_mode(lv_event_t *e)
{
    (void)e;
    nvs_set_color_mode((nvs_get_color_mode() + 1) % COLOR_MODE_COUNT);
    settings_save();
    refresh_player_ui();
    refresh_player_settings_ui();
}

static void event_gm_cmd_marker(lv_event_t *e)
{
    (void)e;
    nvs_set_cmd_marker_mode((nvs_get_cmd_marker_mode() + 1) % CMD_MARKER_COUNT);
    settings_save();
    refresh_player_ui();
    refresh_player_settings_ui();
}

static void event_gm_orientation(lv_event_t *e)
{
    (void)e;
    nvs_set_orientation((nvs_get_orientation() + 1) % ORIENTATION_MODE_COUNT);
    settings_save();
    refresh_player_ui();
    refresh_player_settings_ui();
}

static void event_gm_name_display(lv_event_t *e)
{
    (void)e;
    nvs_set_turn_show_name(!nvs_get_turn_show_name());
    settings_save();
    refresh_turn_ui();
    refresh_player_settings_ui();
}

static void event_gm_open_player_settings(lv_event_t *e)
{
    (void)e;
    refresh_player_settings_ui();
    lv_scr_load(screen_game_player_settings);
}

static void event_gm_player_back(lv_event_t *e)
{
    (void)e;
    return_to_game_mode_menu();
}

static void event_gm_open_timer_settings(lv_event_t *e)
{
    (void)e;
    open_turn_timer_settings_from_game();
}

static void event_gm_apply(lv_event_t *e)
{
    (void)e;

    /*
     * Apply edits the current match in place. Life values, counters, timer,
     * turn history and event log stay intact; only configuration/layout is
     * committed.
     */
    nvs_set_num_players(temp_num_players);
    nvs_set_players_to_track(temp_players_to_track);
    nvs_set_life_total(temp_life_total);
    settings_save();

    if (temp_players_to_track > 1)
        rebuild_multiplayer_layout(temp_players_to_track);

    back_to_main();
    if (lv_indev_get_act() != NULL)
        lv_indev_wait_release(lv_indev_get_act());
}

// ---------- screen builders ----------
void build_game_mode_menu_screen(void)
{
    lv_obj_t *btn;

    /* Game Settings hub: setup, player presentation, timer, apply. */
    {
        quad_item_t items[4] = {
            {"Game\nSetup",      event_gm_open_setup,           true, LV_EVENT_CLICKED},
            {"Player\nSettings", event_gm_open_player_settings, true, LV_EVENT_CLICKED},
            {"Timer\nSettings",  event_gm_open_timer_settings,  true, LV_EVENT_CLICKED},
            {"Apply\n(Hold)",    event_gm_apply,                true, LV_EVENT_LONG_PRESSED},
        };
        build_quad_screen(&screen_game_mode_menu, items);
    }

    /* Existing per-match setup controls live one level below the hub. */
    {
        quad_item_t items[4] = {
            {"Players\n4", event_gm_num_players,      true, LV_EVENT_CLICKED},
            {"Track\n4",   event_gm_players_to_track, true, LV_EVENT_CLICKED},
            {"Life\n40",   event_gm_life_cycle,       true, LV_EVENT_SHORT_CLICKED},
            {"Back",        event_gm_setup_back,       true, LV_EVENT_CLICKED},
        };
        build_quad_screen(&screen_game_setup, items);

        btn = lv_obj_get_child(screen_game_setup, 0);
        label_gm_num_players = lv_obj_get_child(btn, 0);
        btn = lv_obj_get_child(screen_game_setup, 1);
        label_gm_players_to_track = lv_obj_get_child(btn, 0);
        btn = lv_obj_get_child(screen_game_setup, 2);
        label_gm_life_total = lv_obj_get_child(btn, 0);

        /* Short tap cycles presets; long press opens exact custom life. */
        lv_obj_add_event_cb(btn, event_gm_life_custom,
                            LV_EVENT_LONG_PRESSED, NULL);
    }

    {
        quad_item_t items[4] = {
            {"Colors\nPlayer",       event_gm_color_mode,    true, LV_EVENT_CLICKED},
            {"Cmd Marker\nDot",      event_gm_cmd_marker,    true, LV_EVENT_CLICKED},
            {"Orientation\nAbsolute",event_gm_orientation,    true, LV_EVENT_CLICKED},
            {"Name Display\nON",     event_gm_name_display,   true, LV_EVENT_CLICKED},
        };
        build_quad_screen(&screen_game_player_settings, items);

        btn = lv_obj_get_child(screen_game_player_settings, 0);
        label_gm_color_mode = lv_obj_get_child(btn, 0);
        btn = lv_obj_get_child(screen_game_player_settings, 1);
        label_gm_cmd_marker = lv_obj_get_child(btn, 0);
        btn = lv_obj_get_child(screen_game_player_settings, 2);
        label_gm_orientation = lv_obj_get_child(btn, 0);
        btn = lv_obj_get_child(screen_game_player_settings, 3);
        label_gm_name_display = lv_obj_get_child(btn, 0);

        /* Long-press on any player-setting tile is not special; edge swipe
           or back navigation returns to the Game Settings hub. */
        (void)event_gm_player_back;
    }

    refresh_game_mode_menu_ui();
}

void build_custom_life_screen(void)
{
    lv_obj_t *title;
    lv_obj_t *hint;

    screen_custom_life = lv_obj_create(NULL);
    lv_obj_set_size(screen_custom_life, 360, 360);
    lv_obj_set_style_bg_color(screen_custom_life, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_custom_life, 0, 0);
    lv_obj_set_scrollbar_mode(screen_custom_life, LV_SCROLLBAR_MODE_OFF);

    title = lv_label_create(screen_custom_life);
    lv_label_set_text(title, "Life Total");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    label_custom_life_value = lv_label_create(screen_custom_life);
    lv_label_set_text(label_custom_life_value, "40");
    lv_obj_set_style_text_color(label_custom_life_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_custom_life_value, &lv_font_montserrat_32, 0);
    lv_obj_align(label_custom_life_value, LV_ALIGN_CENTER, 0, -10);

    hint = lv_label_create(screen_custom_life);
    lv_label_set_text(hint, "Turn knob to adjust");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x6A6A6A), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 24);
}
