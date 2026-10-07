#include "postgame.h"
#include "game.h"
#include "storage.h"
#include "pregame.h"
#include "ui_mp.h"
#include "playgroup_pending.h"

extern void back_to_main(void);

lv_obj_t *screen_postgame_winner = NULL;
lv_obj_t *screen_postgame_wincon = NULL;
lv_obj_t *screen_postgame_infinite = NULL;

typedef struct {
    const char *label;
    const char *api_token;
} postgame_wincon_t;

static const postgame_wincon_t win_conditions[] = {
    {"COMBAT", "combat"},
    {"COMBO", "combo"},
    {"COMMANDER\nDAMAGE", "commander_damage"},
    {"POISON", "poison"},
    {"MILL", "mill"},
    {"ALTERNATIVE", "alternative"},
    {"NON-COMBAT\nDAMAGE", "non_combat_damage"},
};

#define POSTGAME_WINCON_COUNT ((int)(sizeof(win_conditions) / sizeof(win_conditions[0])))

static lv_obj_t *winner_art = NULL;
static lv_obj_t *winner_overlay = NULL;
static lv_obj_t *winner_name = NULL;
static lv_obj_t *winner_meta = NULL;
static lv_obj_t *wincon_name = NULL;
static lv_obj_t *wincon_meta = NULL;
static lv_obj_t *infinite_name = NULL;
static lv_timer_t *postgame_watch_timer = NULL;

static int winner_index = 0;
static int wincon_index = 0;
static int infinite_index = 0;
static bool questionnaire_open = false;
static bool questionnaire_dismissed = false;

static lv_obj_t *postgame_button(lv_obj_t *parent, const char *text,
                                 lv_coord_t width, lv_coord_t height,
                                 lv_event_cb_t cb)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_radius(button, height / 2, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x17212A), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x6F8398), 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    return button;
}

static void make_title(lv_obj_t *screen, const char *title, const char *hint)
{
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 36);

    label = lv_label_create(screen);
    lv_label_set_text(label, hint);
    lv_obj_set_style_text_color(label, lv_color_hex(0x7E8A96), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 70);
}

static int tracked_player_count(void)
{
    int count = nvs_get_players_to_track();
    if (count < 1) count = 1;
    if (count > MAX_DISPLAY_PLAYERS) count = MAX_DISPLAY_PLAYERS;
    return count;
}

static int inferred_winner(void)
{
    int count = tracked_player_count();
    int alive = 0;
    int winner = -1;

    for (int i = 0; i < count; i++) {
        if (!player_eliminated[i]) {
            alive++;
            winner = i;
        }
    }
    return alive == 1 ? winner : -1;
}

static int infer_win_condition(int winner)
{
    int count = tracked_player_count();

    if (winner >= 0 && winner < count) {
        for (int target = 0; target < count; target++) {
            if (target != winner &&
                cmd_damage_totals[winner][target] >= 21)
                return 2; /* Commander Damage */
        }
    }

    for (int target = 0; target < count; target++) {
        if (target != winner &&
            player_counters[target][COUNTER_TYPE_POISON] >= 10)
            return 3; /* Poison */
    }

    return 0; /* Combat is only a UI default, always user-confirmed. */
}

static void refresh_winner(void)
{
    const lv_img_dsc_t *art;
    char meta[32];

    if (winner_index < 0 || winner_index >= tracked_player_count())
        winner_index = 0;

    art = pregame_get_player_commander_art(winner_index);
    if (winner_art != NULL) {
        if (art != NULL) {
            lv_img_set_src(winner_art, art);
            lv_obj_clear_flag(winner_art, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(winner_art, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (winner_name != NULL)
        lv_label_set_text(winner_name, player_names[winner_index]);

    if (winner_meta != NULL) {
        snprintf(meta, sizeof(meta), "PLAYER %d OF %d",
                 winner_index + 1, tracked_player_count());
        lv_label_set_text(winner_meta, meta);
    }
}

static void refresh_wincon(void)
{
    char meta[20];

    if (wincon_index < 0) wincon_index = POSTGAME_WINCON_COUNT - 1;
    if (wincon_index >= POSTGAME_WINCON_COUNT) wincon_index = 0;

    if (wincon_name != NULL)
        lv_label_set_text(wincon_name, win_conditions[wincon_index].label);

    if (wincon_meta != NULL) {
        snprintf(meta, sizeof(meta), "%d / %d",
                 wincon_index + 1, POSTGAME_WINCON_COUNT);
        lv_label_set_text(wincon_meta, meta);
    }
}

static void refresh_infinite(void)
{
    if (infinite_name == NULL)
        return;

    lv_label_set_text(infinite_name, infinite_index ? "YES" : "NO");
    lv_obj_set_style_text_color(
        infinite_name,
        infinite_index ? lv_color_hex(0x8EE6D6) : lv_color_white(), 0);
}

static void event_winner_confirm(lv_event_t *event)
{
    (void)event;
    wincon_index = infer_win_condition(winner_index);
    refresh_wincon();
    lv_scr_load(screen_postgame_wincon);
}

static void event_winner_back(lv_event_t *event)
{
    (void)event;
    questionnaire_open = false;
    questionnaire_dismissed = true;
    back_to_main();
}

static void event_wincon_confirm(lv_event_t *event)
{
    (void)event;
    infinite_index = 0;
    refresh_infinite();
    lv_scr_load(screen_postgame_infinite);
}

static void event_infinite_confirm(lv_event_t *event)
{
    (void)event;

    playgroup_pending_set_result(
        winner_index,
        win_conditions[wincon_index].api_token,
        infinite_index != 0);

    questionnaire_open = false;
    questionnaire_dismissed = false;

    printf("[Playgroup] Result confirmed: winner P%d, win condition %s, infinite=%s\n",
           winner_index + 1,
           win_conditions[wincon_index].api_token,
           infinite_index ? "yes" : "no");

    back_to_main();
}

static void postgame_watch_cb(lv_timer_t *timer)
{
    int winner;
    (void)timer;

    if (!playgroup_pending_current_active() ||
        playgroup_pending_result_confirmed()) {
        questionnaire_open = false;
        questionnaire_dismissed = false;
        return;
    }

    winner = inferred_winner();
    if (winner < 0) {
        questionnaire_open = false;
        questionnaire_dismissed = false;
        return;
    }

    if (questionnaire_open || questionnaire_dismissed)
        return;

    /* Don't replace menus/overlays mid-interaction. Wait until the table
       itself is visible; the next timer tick will offer the questionnaire. */
    if (lv_scr_act() != screen_multiplayer)
        return;

    winner_index = winner;
    wincon_index = infer_win_condition(winner);
    infinite_index = 0;
    questionnaire_open = true;

    refresh_winner();
    lv_scr_load(screen_postgame_winner);
}

void postgame_change_selection(int delta)
{
    if (delta == 0)
        return;

    if (lv_scr_act() == screen_postgame_winner) {
        int count = tracked_player_count();
        winner_index += (delta < 0) ? -1 : 1;
        if (winner_index < 0) winner_index = count - 1;
        if (winner_index >= count) winner_index = 0;
        refresh_winner();
    } else if (lv_scr_act() == screen_postgame_wincon) {
        wincon_index += (delta < 0) ? -1 : 1;
        if (wincon_index < 0) wincon_index = POSTGAME_WINCON_COUNT - 1;
        if (wincon_index >= POSTGAME_WINCON_COUNT) wincon_index = 0;
        refresh_wincon();
    } else if (lv_scr_act() == screen_postgame_infinite) {
        infinite_index = infinite_index ? 0 : 1;
        refresh_infinite();
    }
}

void build_postgame_screens(void)
{
    lv_obj_t *button;

    screen_postgame_winner = lv_obj_create(NULL);
    lv_obj_set_size(screen_postgame_winner, 360, 360);
    lv_obj_set_style_bg_color(screen_postgame_winner, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_postgame_winner, 0, 0);
    lv_obj_clear_flag(screen_postgame_winner, LV_OBJ_FLAG_SCROLLABLE);

    winner_art = lv_img_create(screen_postgame_winner);
    lv_obj_align(winner_art, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(winner_art, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(winner_art, LV_OBJ_FLAG_CLICKABLE);

    winner_overlay = lv_obj_create(screen_postgame_winner);
    lv_obj_remove_style_all(winner_overlay);
    lv_obj_set_size(winner_overlay, 360, 360);
    lv_obj_set_style_bg_color(winner_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(winner_overlay, LV_OPA_60, 0);
    lv_obj_clear_flag(winner_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(winner_overlay, LV_ALIGN_CENTER, 0, 0);

    make_title(screen_postgame_winner, "WHO WON?", "Turn dial to choose");

    winner_name = lv_label_create(screen_postgame_winner);
    lv_obj_set_width(winner_name, 270);
    lv_label_set_long_mode(winner_name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(winner_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(winner_name, lv_color_white(), 0);
    lv_obj_set_style_text_font(winner_name, &lv_font_montserrat_32, 0);
    lv_obj_align(winner_name, LV_ALIGN_CENTER, 0, -18);

    winner_meta = lv_label_create(screen_postgame_winner);
    lv_obj_set_style_text_color(winner_meta, lv_color_hex(0x9AA6B2), 0);
    lv_obj_set_style_text_font(winner_meta, &lv_font_montserrat_14, 0);
    lv_obj_align(winner_meta, LV_ALIGN_CENTER, 0, 22);

    button = postgame_button(screen_postgame_winner, "CONFIRM", 150, 44,
                             event_winner_confirm);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -38);

    button = postgame_button(screen_postgame_winner, "BACK TO GAME", 130, 32,
                             event_winner_back);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(button, 0),
                                lv_color_hex(0x7E8A96), 0);
    lv_obj_set_style_text_font(lv_obj_get_child(button, 0),
                               &lv_font_montserrat_14, 0);

    screen_postgame_wincon = lv_obj_create(NULL);
    lv_obj_set_size(screen_postgame_wincon, 360, 360);
    lv_obj_set_style_bg_color(screen_postgame_wincon, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_postgame_wincon, 0, 0);
    lv_obj_clear_flag(screen_postgame_wincon, LV_OBJ_FLAG_SCROLLABLE);

    make_title(screen_postgame_wincon, "HOW DID IT END?", "Turn dial to choose");

    wincon_name = lv_label_create(screen_postgame_wincon);
    lv_obj_set_width(wincon_name, 300);
    lv_obj_set_style_text_align(wincon_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(wincon_name, lv_color_hex(0x8EE6D6), 0);
    lv_obj_set_style_text_font(wincon_name, &lv_font_montserrat_32, 0);
    lv_obj_align(wincon_name, LV_ALIGN_CENTER, 0, -16);

    wincon_meta = lv_label_create(screen_postgame_wincon);
    lv_obj_set_style_text_color(wincon_meta, lv_color_hex(0x7E8A96), 0);
    lv_obj_set_style_text_font(wincon_meta, &lv_font_montserrat_14, 0);
    lv_obj_align(wincon_meta, LV_ALIGN_CENTER, 0, 34);

    button = postgame_button(screen_postgame_wincon, "CONFIRM", 150, 44,
                             event_wincon_confirm);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -36);

    screen_postgame_infinite = lv_obj_create(NULL);
    lv_obj_set_size(screen_postgame_infinite, 360, 360);
    lv_obj_set_style_bg_color(screen_postgame_infinite, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_postgame_infinite, 0, 0);
    lv_obj_clear_flag(screen_postgame_infinite, LV_OBJ_FLAG_SCROLLABLE);

    make_title(screen_postgame_infinite, "WENT INFINITE?", "Turn dial: No / Yes");

    infinite_name = lv_label_create(screen_postgame_infinite);
    lv_obj_set_style_text_font(infinite_name, &lv_font_montserrat_bold_44, 0);
    lv_obj_align(infinite_name, LV_ALIGN_CENTER, 0, -8);

    button = postgame_button(screen_postgame_infinite, "SAVE RESULT", 170, 44,
                             event_infinite_confirm);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -42);

    refresh_wincon();
    refresh_infinite();

    postgame_watch_timer = lv_timer_create(postgame_watch_cb, 300, NULL);
}
