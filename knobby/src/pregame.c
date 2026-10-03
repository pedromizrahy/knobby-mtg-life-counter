#include "pregame.h"
#include "storage.h"
#include "game.h"
#include "ui_mp.h"
#include "ui_1p.h"

extern void reset_all_values(void);
extern void back_to_main(void);

lv_obj_t *screen_pregame_home = NULL;
lv_obj_t *screen_pregame_players = NULL;
lv_obj_t *screen_pregame_roster = NULL;
lv_obj_t *screen_pregame_mulligans = NULL;

static int pregame_player_count = 4;
static uint8_t mulligans[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *roster_labels[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *mulligan_labels[MAX_DISPLAY_PLAYERS] = {0};

static void refresh_roster(void);
static void refresh_mulligans(void);

static lv_obj_t *pregame_button(lv_obj_t *parent, const char *text,
                                lv_coord_t w, lv_coord_t h,
                                lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, h / 2, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x121820), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x4A5563), 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    {
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_center(label);
    }
    return btn;
}

static void event_track_new_game(lv_event_t *e)
{
    (void)e;
    pregame_player_count = nvs_get_players_to_track();
    if (pregame_player_count < 1 || pregame_player_count > MAX_DISPLAY_PLAYERS)
        pregame_player_count = 4;
    lv_scr_load(screen_pregame_players);
}

static void event_choose_players(lv_event_t *e)
{
    int count = (int)(intptr_t)lv_event_get_user_data(e);

    if (count < 1 || count > MAX_DISPLAY_PLAYERS) return;
    pregame_player_count = count;
    refresh_roster();
    lv_scr_load(screen_pregame_roster);
}

static void refresh_roster(void)
{
    int i;
    char buf[48];

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        if (roster_labels[i] == NULL) continue;
        if (i < pregame_player_count) {
            snprintf(buf, sizeof(buf), "%s   |   Deck %d", player_names[i], i + 1);
            lv_label_set_text(roster_labels[i], buf);
            lv_obj_clear_flag(roster_labels[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(roster_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void event_roster_continue(lv_event_t *e)
{
    int i;
    (void)e;

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) mulligans[i] = 0;
    refresh_mulligans();
    lv_scr_load(screen_pregame_mulligans);
}

static void refresh_mulligans(void)
{
    int i;
    char buf[48];

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        if (mulligan_labels[i] == NULL) continue;

        if (i < pregame_player_count) {
            snprintf(buf, sizeof(buf), "%s   Mulligan %u",
                     player_names[i], (unsigned)mulligans[i]);
            lv_label_set_text(mulligan_labels[i], buf);
            lv_obj_clear_flag(lv_obj_get_parent(mulligan_labels[i]), LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lv_obj_get_parent(mulligan_labels[i]), LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void event_mulligan_cycle(lv_event_t *e)
{
    int player = (int)(intptr_t)lv_event_get_user_data(e);

    if (player < 0 || player >= pregame_player_count) return;
    mulligans[player] = (uint8_t)((mulligans[player] + 1U) % 8U);
    refresh_mulligans();
}

static void event_start_game(lv_event_t *e)
{
    int i;
    (void)e;

    nvs_set_num_players(pregame_player_count);
    nvs_set_players_to_track(pregame_player_count);
    settings_save();

    for (i = 0; i < pregame_player_count; i++) {
        snprintf(player_names[i], sizeof(player_names[i]), "P%d", i + 1);
    }

    rebuild_multiplayer_layout(pregame_player_count);
    reset_all_values();
    back_to_main();
}

void open_pregame_home(void)
{
    if (screen_pregame_home != NULL)
        lv_scr_load(screen_pregame_home);
}

bool pregame_handle_back(lv_obj_t *screen)
{
    if (screen == screen_pregame_players) {
        lv_scr_load(screen_pregame_home);
        return true;
    }
    if (screen == screen_pregame_roster) {
        lv_scr_load(screen_pregame_players);
        return true;
    }
    if (screen == screen_pregame_mulligans) {
        lv_scr_load(screen_pregame_roster);
        return true;
    }
    return false;
}

void build_pregame_screens(void)
{
    int i;

    screen_pregame_home = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_home, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_home, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_home, 0, 0);

    {
        lv_obj_t *eyebrow = lv_label_create(screen_pregame_home);
        lv_label_set_text(eyebrow, "DIAL DOS PRIMOS");
        lv_obj_set_style_text_color(eyebrow, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_14, 0);
        lv_obj_align(eyebrow, LV_ALIGN_CENTER, 0, -78);

        lv_obj_t *title = lv_label_create(screen_pregame_home);
        lv_label_set_text(title, "TRACK\nNEW GAME");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

        lv_obj_t *btn = pregame_button(screen_pregame_home, "START", 132, 46,
                                       event_track_new_game);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, 78);
    }

    screen_pregame_players = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_players, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_players, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_players, 0, 0);

    {
        lv_obj_t *title = lv_label_create(screen_pregame_players);
        lv_label_set_text(title, "PLAYERS");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);
    }

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        char buf[4];
        lv_obj_t *btn;
        snprintf(buf, sizeof(buf), "%d", i + 1);
        btn = pregame_button(screen_pregame_players, buf, 64, 52,
                             event_choose_players);
        lv_obj_add_event_cb(btn, event_choose_players, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(i + 1));
        /* Remove the callback added by helper to avoid duplicate user-data-less call. */
        lv_obj_remove_event_cb(btn, event_choose_players);
        lv_obj_add_event_cb(btn, event_choose_players, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(i + 1));
        lv_obj_align(btn, LV_ALIGN_CENTER,
                     (i % 2 == 0) ? -42 : 42,
                     (i < 2) ? -28 : 40);
    }

    screen_pregame_roster = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_roster, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_roster, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_roster, 0, 0);

    {
        lv_obj_t *title = lv_label_create(screen_pregame_roster);
        lv_label_set_text(title, "PLAYERS & DECKS");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

        for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
            roster_labels[i] = lv_label_create(screen_pregame_roster);
            lv_label_set_text(roster_labels[i], "P1   |   Deck 1");
            lv_obj_set_style_text_color(roster_labels[i], lv_color_hex(0xD4DCE4), 0);
            lv_obj_set_style_text_font(roster_labels[i], &lv_font_montserrat_16, 0);
            lv_obj_align(roster_labels[i], LV_ALIGN_TOP_MID, 0, 78 + (i * 38));
        }

        lv_obj_t *next = pregame_button(screen_pregame_roster, "MULLIGANS", 142, 42,
                                        event_roster_continue);
        lv_obj_align(next, LV_ALIGN_BOTTOM_MID, 0, -30);
    }

    screen_pregame_mulligans = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_mulligans, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_mulligans, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_mulligans, 0, 0);

    {
        lv_obj_t *title = lv_label_create(screen_pregame_mulligans);
        lv_label_set_text(title, "MULLIGANS");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

        for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
            lv_obj_t *btn = pregame_button(screen_pregame_mulligans, "", 206, 38,
                                           event_mulligan_cycle);
            lv_obj_remove_event_cb(btn, event_mulligan_cycle);
            lv_obj_add_event_cb(btn, event_mulligan_cycle, LV_EVENT_CLICKED,
                                (void *)(intptr_t)i);
            lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 70 + (i * 44));
            mulligan_labels[i] = lv_obj_get_child(btn, 0);
        }

        lv_obj_t *start = pregame_button(screen_pregame_mulligans, "START GAME", 146, 42,
                                         event_start_game);
        lv_obj_align(start, LV_ALIGN_BOTTOM_MID, 0, -24);
    }

    refresh_roster();
    refresh_mulligans();
}
