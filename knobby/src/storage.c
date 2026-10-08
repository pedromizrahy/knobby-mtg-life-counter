#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

// ---------- cached state ----------
static bool settings_dirty = false;
static int cached_brightness = DEFAULT_BRIGHTNESS_PERCENT;
static int cached_auto_dim = AUTO_DIM_OFF;
static int cached_color_mode = 0;
static int runtime_color_mode_override = -1;
static int cached_deselect_timeout = 0; /* index: 0=never, 1=5s, 2=15s, 3=30s */
static int cached_orientation = ORIENTATION_MODE_ABSOLUTE;
static int cached_display_rotation = 0; /* physical rotation, degrees = value * 90 */
static int cached_menu_facing = 0; /* 0=Fixed (default), 1=Face Player */
static int cached_num_players = 4;
static int cached_players_to_track = 1;
static int cached_life_total = DEFAULT_LIFE_TOTAL;
static int cached_auto_eliminate = 1; /* 1=ON (default), 0=OFF */
static int cached_random_first = 1; /* 1=ON (default): random first-player pick on reset */
static int cached_turn_timer_enabled = 1; /* 1=ON: start turn tracking with a new game */
static int cached_turn_show_name = 0; /* compact timer by default */
static int cached_turn_reminder_minutes = 5; /* 0 disables reminder */
static int cached_turn_alert_duration_seconds = 3;
static int cached_turn_visual_alert = 1;
static int cached_timer_face_player = 1;
static int cached_cmd_marker_mode = CMD_MARKER_DOT;
static int cached_pizza_art = 0;
static int cached_multi_select = 0; /* 0=OFF (default), 1=ON */
static char cached_name_list[NAME_LIST_COUNT][NAME_LIST_LEN];

// ---------- init ----------
void knob_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t handle;
    if (nvs_open("knobby", NVS_READONLY, &handle) == ESP_OK) {
        int8_t dim_val = 0;
        int8_t bri_val = DEFAULT_BRIGHTNESS_PERCENT;
        int8_t lc_val = 0;
        int8_t dt_val = 0;
        int8_t rot_val = 0;
        int8_t dr_val = 0;
        int8_t np_val = 4;
        int8_t pt_val = 1;
        int16_t lt_val = DEFAULT_LIFE_TOTAL;

        nvs_get_i8(handle, "auto_dim", &dim_val);
        nvs_get_i8(handle, "brightness", &bri_val);
        nvs_get_i8(handle, "color_mode", &lc_val);
        nvs_get_i8(handle, "desel_time", &dt_val);
        nvs_get_i8(handle, "rotation", &rot_val);
        nvs_get_i8(handle, "disp_rot", &dr_val);
        nvs_get_i8(handle, "num_players", &np_val);
        nvs_get_i8(handle, "track", &pt_val);
        nvs_get_i16(handle, "life_total", &lt_val);

        cached_auto_dim = (dim_val < 0) ? AUTO_DIM_OFF : (dim_val >= AUTO_DIM_COUNT) ? AUTO_DIM_OFF : dim_val;
        cached_color_mode = (lc_val < 0) ? COLOR_MODE_PLAYER : (lc_val >= COLOR_MODE_COUNT) ? COLOR_MODE_PLAYER : lc_val;
        cached_deselect_timeout = (dt_val < 0) ? 0 : (dt_val > 3) ? 3 : dt_val;
        cached_orientation = (rot_val < 0) ? ORIENTATION_MODE_ABSOLUTE
                : (rot_val >= ORIENTATION_MODE_COUNT) ? ORIENTATION_MODE_ABSOLUTE
                                   : rot_val;
        cached_display_rotation = (dr_val < 0 || dr_val >= DISPLAY_ROTATION_COUNT) ? 0 : dr_val;
        cached_brightness = clamp_brightness(bri_val);
        cached_num_players = (np_val < 1) ? 1 : (np_val > MAX_GAME_PLAYERS) ? MAX_GAME_PLAYERS : np_val;
        cached_players_to_track = (pt_val < 1) ? 1 : (pt_val > MAX_DISPLAY_PLAYERS) ? MAX_DISPLAY_PLAYERS : pt_val;
        /* Every consumer assumes track <= num_players; foreign/old NVS could
           store an inconsistent pair that the per-field clamps above allow. */
        if (cached_players_to_track > cached_num_players)
            cached_players_to_track = cached_num_players;
        cached_life_total = (lt_val < 1) ? 1 : (lt_val > LIFE_MAX) ? LIFE_MAX : lt_val;

        int8_t ae_val = 1;
        nvs_get_i8(handle, "auto_elim", &ae_val);
        cached_auto_eliminate = (ae_val != 0) ? 1 : 0;

        int8_t rf_val = 1;
        nvs_get_i8(handle, "rand_first", &rf_val);
        cached_random_first = (rf_val != 0) ? 1 : 0;

        int8_t tt_val = 1;
        int8_t tn_val = 0;
        int8_t tr_val = 5;
        int8_t tad_val = 3;
        int8_t tv_val = 1;
        int8_t tf_val = 1;
        int8_t cm_val = CMD_MARKER_DOT;
        int8_t pa_val = 0;
        nvs_get_i8(handle, "turn_timer", &tt_val);
        nvs_get_i8(handle, "turn_name", &tn_val);
        nvs_get_i8(handle, "turn_rem", &tr_val);
        nvs_get_i8(handle, "turn_alert_s", &tad_val);
        nvs_get_i8(handle, "turn_visual", &tv_val);
        nvs_get_i8(handle, "timer_face", &tf_val);
        nvs_get_i8(handle, "cmd_marker", &cm_val);
        nvs_get_i8(handle, "pizza_art", &pa_val);
        cached_turn_timer_enabled = (tt_val != 0) ? 1 : 0;
        cached_turn_show_name = (tn_val != 0) ? 1 : 0;
        cached_turn_reminder_minutes =
            (tr_val == 0 || tr_val == 3 || tr_val == 5 || tr_val == 10 || tr_val == 15)
                ? tr_val : 5;
        cached_turn_alert_duration_seconds =
            (tad_val == 3 || tad_val == 5 || tad_val == 10) ? tad_val : 3;
        cached_turn_visual_alert = (tv_val != 0) ? 1 : 0;
        cached_timer_face_player = (tf_val != 0) ? 1 : 0;
        cached_cmd_marker_mode =
            (cm_val < 0 || cm_val >= CMD_MARKER_COUNT) ? CMD_MARKER_DOT : cm_val;
        cached_pizza_art = (pa_val != 0) ? 1 : 0;

        int8_t ms_val = 0;
        nvs_get_i8(handle, "multi_sel", &ms_val);
        cached_multi_select = (ms_val != 0) ? 1 : 0;

        int8_t mf_val = 0;
        nvs_get_i8(handle, "menu_face", &mf_val);
        cached_menu_facing = (mf_val != 0) ? 1 : 0;

        size_t nl_size = sizeof(cached_name_list);
        nvs_get_blob(handle, "name_list", cached_name_list, &nl_size);
        for (int i = 0; i < NAME_LIST_COUNT; i++)
            cached_name_list[i][NAME_LIST_LEN - 1] = '\0';

        nvs_close(handle);
    }
}

// ---------- getters ----------
int nvs_get_brightness(void)
{
    return cached_brightness;
}

int nvs_get_auto_dim(void)
{
    return cached_auto_dim;
}

// ---------- setters ----------
void nvs_set_brightness(int value)
{
    cached_brightness = clamp_brightness(value);
    settings_dirty = true;
}

void nvs_set_auto_dim(int value)
{
    cached_auto_dim = (value < 0) ? AUTO_DIM_OFF : (value >= AUTO_DIM_COUNT) ? AUTO_DIM_OFF : value;
    settings_dirty = true;
}

int nvs_get_color_mode(void)
{
    return (runtime_color_mode_override >= 0)
        ? runtime_color_mode_override
        : cached_color_mode;
}

void nvs_set_color_mode(int value)
{
    cached_color_mode = (value < 0) ? COLOR_MODE_PLAYER : (value >= COLOR_MODE_COUNT) ? COLOR_MODE_PLAYER : value;
    settings_dirty = true;
}

void nvs_set_color_mode_runtime_override(int value)
{
    runtime_color_mode_override =
        (value < 0 || value >= COLOR_MODE_COUNT) ? COLOR_MODE_PLAYER : value;
}

void nvs_clear_color_mode_runtime_override(void)
{
    runtime_color_mode_override = -1;
}

int nvs_get_deselect_timeout(void)
{
    return cached_deselect_timeout;
}

void nvs_set_deselect_timeout(int value)
{
    cached_deselect_timeout = (value < 0) ? 0 : (value > 3) ? 3 : value;
    settings_dirty = true;
}

int nvs_get_orientation(void)
{
    return cached_orientation;
}

void nvs_set_orientation(int value)
{
    cached_orientation = (value < 0) ? ORIENTATION_MODE_ABSOLUTE
                                      : (value >= ORIENTATION_MODE_COUNT) ? ORIENTATION_MODE_ABSOLUTE
                                                                       : value;
    settings_dirty = true;
}

int nvs_get_display_rotation(void)
{
    return cached_display_rotation;
}

void nvs_set_display_rotation(int value)
{
    cached_display_rotation = (value < 0 || value >= DISPLAY_ROTATION_COUNT) ? 0 : value;
    settings_dirty = true;
}

// ---------- menu facing ----------
int nvs_get_menu_facing(void)
{
    return cached_menu_facing;
}

void nvs_set_menu_facing(int value)
{
    cached_menu_facing = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

// ---------- game mode getters/setters ----------
int nvs_get_num_players(void)
{
    return cached_num_players;
}

void nvs_set_num_players(int value)
{
    cached_num_players = (value < 1) ? 1 : (value > MAX_GAME_PLAYERS) ? MAX_GAME_PLAYERS : value;
    settings_dirty = true;
}

int nvs_get_players_to_track(void)
{
    return cached_players_to_track;
}

void nvs_set_players_to_track(int value)
{
    cached_players_to_track = (value < 1) ? 1 : (value > MAX_DISPLAY_PLAYERS) ? MAX_DISPLAY_PLAYERS : value;
    settings_dirty = true;
}

int nvs_get_life_total(void)
{
    return cached_life_total;
}

void nvs_set_life_total(int value)
{
    cached_life_total = (value < 1) ? 1 : (value > LIFE_MAX) ? LIFE_MAX : value;
    settings_dirty = true;
}

// ---------- auto-eliminate ----------
int nvs_get_auto_eliminate(void)
{
    return cached_auto_eliminate;
}

void nvs_set_auto_eliminate(int value)
{
    cached_auto_eliminate = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

// ---------- random first-player pick ----------
int nvs_get_random_first(void)
{
    return cached_random_first;
}

void nvs_set_random_first(int value)
{
    cached_random_first = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

// ---------- turn timer ----------
int nvs_get_turn_timer_enabled(void)
{
    return cached_turn_timer_enabled;
}

void nvs_set_turn_timer_enabled(int value)
{
    cached_turn_timer_enabled = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

int nvs_get_turn_show_name(void)
{
    return cached_turn_show_name;
}

void nvs_set_turn_show_name(int value)
{
    cached_turn_show_name = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

int nvs_get_turn_reminder_minutes(void)
{
    return cached_turn_reminder_minutes;
}

void nvs_set_turn_reminder_minutes(int value)
{
    if (value != 0 && value != 3 && value != 5 && value != 10 && value != 15)
        value = 5;
    cached_turn_reminder_minutes = value;
    settings_dirty = true;
}

int nvs_get_turn_alert_duration_seconds(void)
{
    return cached_turn_alert_duration_seconds;
}

void nvs_set_turn_alert_duration_seconds(int value)
{
    if (value != 3 && value != 5 && value != 10) value = 3;
    cached_turn_alert_duration_seconds = value;
    settings_dirty = true;
}

int nvs_get_turn_visual_alert(void)
{
    return cached_turn_visual_alert;
}

void nvs_set_turn_visual_alert(int value)
{
    cached_turn_visual_alert = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

int nvs_get_timer_face_player(void)
{
    return cached_timer_face_player;
}

void nvs_set_timer_face_player(int value)
{
    cached_timer_face_player = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

int nvs_get_cmd_marker_mode(void)
{
    return cached_cmd_marker_mode;
}

void nvs_set_cmd_marker_mode(int value)
{
    cached_cmd_marker_mode =
        (value < 0 || value >= CMD_MARKER_COUNT) ? CMD_MARKER_DOT : value;
    settings_dirty = true;
}

int nvs_get_pizza_art(void)
{
    return cached_pizza_art;
}

void nvs_set_pizza_art(int value)
{
    cached_pizza_art = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

// ---------- multi-select ----------
int nvs_get_multi_select(void)
{
    return cached_multi_select;
}

void nvs_set_multi_select(int value)
{
    cached_multi_select = (value != 0) ? 1 : 0;
    settings_dirty = true;
}

// ---------- name list ----------
void nvs_get_name_list(char (*out)[NAME_LIST_LEN])
{
    memcpy(out, cached_name_list, sizeof(cached_name_list));
}

void nvs_set_name_list(const char (*list)[NAME_LIST_LEN])
{
    memcpy(cached_name_list, list, sizeof(cached_name_list));
    settings_dirty = true;
}

// ---------- persist ----------
void settings_save(void)
{
    if (!settings_dirty) return;
    nvs_handle_t handle;
    if (nvs_open("knobby", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i8(handle, "auto_dim", (int8_t)cached_auto_dim);
        nvs_set_i8(handle, "brightness", (int8_t)cached_brightness);
        nvs_set_i8(handle, "color_mode", (int8_t)cached_color_mode);
        nvs_set_i8(handle, "desel_time", (int8_t)cached_deselect_timeout);
        nvs_set_i8(handle, "rotation", (int8_t)cached_orientation);
        nvs_set_i8(handle, "disp_rot", (int8_t)cached_display_rotation);
        nvs_set_i8(handle, "num_players", (int8_t)cached_num_players);
        nvs_set_i8(handle, "track", (int8_t)cached_players_to_track);
        nvs_set_i16(handle, "life_total", (int16_t)cached_life_total);
        nvs_set_i8(handle, "auto_elim", (int8_t)cached_auto_eliminate);
        nvs_set_i8(handle, "rand_first", (int8_t)cached_random_first);
        nvs_set_i8(handle, "turn_timer", (int8_t)cached_turn_timer_enabled);
        nvs_set_i8(handle, "turn_name", (int8_t)cached_turn_show_name);
        nvs_set_i8(handle, "turn_rem", (int8_t)cached_turn_reminder_minutes);
        nvs_set_i8(handle, "turn_alert_s", (int8_t)cached_turn_alert_duration_seconds);
        nvs_set_i8(handle, "turn_visual", (int8_t)cached_turn_visual_alert);
        nvs_set_i8(handle, "timer_face", (int8_t)cached_timer_face_player);
        nvs_set_i8(handle, "cmd_marker", (int8_t)cached_cmd_marker_mode);
        nvs_set_i8(handle, "pizza_art", (int8_t)cached_pizza_art);
        nvs_set_i8(handle, "multi_sel", (int8_t)cached_multi_select);
        nvs_set_i8(handle, "menu_face", (int8_t)cached_menu_facing);
        nvs_set_blob(handle, "name_list", cached_name_list, sizeof(cached_name_list));
        esp_err_t commit_err = nvs_commit(handle);
        nvs_close(handle);
        /* Keep the dirty flag set if the commit failed (e.g. NVS full) so a
           later save retries instead of silently dropping the change. */
        if (commit_err == ESP_OK) {
            settings_dirty = false;
        }
    }
}
