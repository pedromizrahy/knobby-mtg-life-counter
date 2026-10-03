#include "ui_damage_resolver.h"
#include "game.h"
#include "storage.h"
#include "ui_1p.h"

lv_obj_t *screen_damage_target = NULL;
lv_obj_t *screen_damage_resolver = NULL;

static lv_obj_t *target_buttons[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *target_labels[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *resolver_title = NULL;
static lv_obj_t *resolver_amount = NULL;
static lv_obj_t *resolver_mode_label = NULL;
static lv_obj_t *resolver_type_buttons[3] = {0};
static lv_obj_t *resolver_more_button = NULL;

static int resolver_source = -1;
static int resolver_target = -1;
static int resolver_amount_value = 0;
static bool resolver_advanced = false;
static game_damage_type_t resolver_damage_type = DAMAGE_TYPE_NORMAL;

static void refresh_target_screen(void)
{
    int track = nvs_get_players_to_track();
    int slot = 0;
    int player;

    for (player = 0; player < MAX_DISPLAY_PLAYERS; player++) {
        if (target_buttons[player] != NULL) {
            lv_obj_add_flag(target_buttons[player], LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (player = 0; player < track && player < MAX_DISPLAY_PLAYERS; player++) {
        if (player == resolver_source) continue;
        if (target_buttons[player] == NULL || target_labels[player] == NULL) continue;

        lv_label_set_text(target_labels[player], player_names[player]);
        lv_obj_set_pos(target_buttons[player], 70, 84 + slot * 58);
        lv_obj_clear_flag(target_buttons[player], LV_OBJ_FLAG_HIDDEN);

        if (player_eliminated[player]) {
            lv_obj_add_state(target_buttons[player], LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(target_buttons[player], LV_STATE_DISABLED);
        }
        slot++;
    }
}

static const char *damage_type_label(game_damage_type_t type)
{
    switch (type) {
    case DAMAGE_TYPE_COMMANDER:
        return "Commander";
    case DAMAGE_TYPE_POISON:
        return "Poison";
    case DAMAGE_TYPE_NORMAL:
    default:
        return "Damage";
    }
}

void refresh_damage_resolver_ui(void)
{
    char buf[64];
    int i;

    if (resolver_title != NULL && resolver_source >= 0 && resolver_target >= 0) {
        snprintf(buf, sizeof(buf), "%s  >  %s",
                 player_names[resolver_source],
                 player_names[resolver_target]);
        lv_label_set_text(resolver_title, buf);
    }

    if (resolver_amount != NULL) {
        snprintf(buf, sizeof(buf), "%d", resolver_amount_value);
        lv_label_set_text(resolver_amount, buf);
    }

    if (resolver_mode_label != NULL) {
        lv_label_set_text(resolver_mode_label,
                          resolver_advanced
                              ? "Choose type"
                              : damage_type_label(resolver_damage_type));
    }

    for (i = 0; i < 3; i++) {
        game_damage_type_t type = (i == 0) ? DAMAGE_TYPE_NORMAL :
                                  (i == 1) ? DAMAGE_TYPE_COMMANDER :
                                             DAMAGE_TYPE_POISON;
        if (resolver_type_buttons[i] == NULL) continue;

        if (resolver_advanced) {
            lv_obj_clear_flag(resolver_type_buttons[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(resolver_type_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_set_style_border_width(resolver_type_buttons[i],
                                      (type == resolver_damage_type) ? 3 : 1, 0);
        lv_obj_set_style_border_color(resolver_type_buttons[i],
                                      (type == resolver_damage_type)
                                          ? lv_color_white()
                                          : lv_color_hex(0x555555), 0);
    }

    if (resolver_more_button != NULL) {
        if (resolver_advanced) {
            lv_obj_add_flag(resolver_more_button, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(resolver_more_button, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void damage_resolver_change_amount(int delta)
{
    resolver_amount_value += delta;
    if (resolver_amount_value < 0) resolver_amount_value = 0;
    if (resolver_amount_value > 999) resolver_amount_value = 999;
    refresh_damage_resolver_ui();
}

void open_damage_resolver_for_target(int source_player, int target_player,
                                     bool advanced)
{
    if (source_player < 0 || source_player >= MAX_DISPLAY_PLAYERS) return;
    if (target_player < 0 || target_player >= MAX_DISPLAY_PLAYERS) return;
    if (source_player == target_player) return;
    if (player_eliminated[source_player] || player_eliminated[target_player]) return;

    resolver_source = source_player;
    resolver_target = target_player;
    resolver_amount_value = 0;
    resolver_advanced = advanced;
    resolver_damage_type = DAMAGE_TYPE_NORMAL;
    refresh_damage_resolver_ui();
    load_screen_if_needed(screen_damage_resolver);
}

static void event_target_select(lv_event_t *e)
{
    int target = (int)(intptr_t)lv_event_get_user_data(e);

    if (target < 0 || target >= MAX_DISPLAY_PLAYERS) return;
    if (target == resolver_source || player_eliminated[target]) return;

    open_damage_resolver_for_target(resolver_source, target, false);
}

static void event_type_select(lv_event_t *e)
{
    int type = (int)(intptr_t)lv_event_get_user_data(e);

    if (type < DAMAGE_TYPE_NORMAL || type > DAMAGE_TYPE_POISON) return;
    resolver_damage_type = (game_damage_type_t)type;
    refresh_damage_resolver_ui();
}

static void event_show_more(lv_event_t *e)
{
    (void)e;
    resolver_advanced = true;
    refresh_damage_resolver_ui();
}

static void event_apply_damage(lv_event_t *e)
{
    (void)e;

    if (resolver_amount_value <= 0) return;
    if (apply_sourced_damage(resolver_source, resolver_target,
                             resolver_amount_value, resolver_damage_type)) {
        back_to_main();
    }
}

void open_damage_resolver(int source_player)
{
    if (source_player < 0 || source_player >= MAX_DISPLAY_PLAYERS) return;
    if (player_eliminated[source_player]) return;

    resolver_source = source_player;
    resolver_target = -1;
    resolver_amount_value = 0;
    resolver_advanced = false;
    resolver_damage_type = DAMAGE_TYPE_NORMAL;
    refresh_target_screen();
    load_screen_if_needed(screen_damage_target);
}

static lv_obj_t *make_type_button(lv_obj_t *parent, const char *text,
                                  lv_coord_t x, game_damage_type_t type)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 94, 42);
    lv_obj_set_pos(btn, x, 222);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn, event_type_select, LV_EVENT_CLICKED,
                        (void *)(intptr_t)type);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    return btn;
}

void build_damage_resolver_screens(void)
{
    int i;

    screen_damage_target = lv_obj_create(NULL);
    lv_obj_set_size(screen_damage_target, 360, 360);
    lv_obj_set_style_bg_color(screen_damage_target, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_damage_target, 0, 0);
    lv_obj_set_scrollbar_mode(screen_damage_target, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(screen_damage_target);
    lv_label_set_text(title, "Choose target");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        target_buttons[i] = lv_btn_create(screen_damage_target);
        lv_obj_set_size(target_buttons[i], 220, 48);
        lv_obj_set_style_radius(target_buttons[i], 10, 0);
        lv_obj_set_style_bg_color(target_buttons[i], lv_color_hex(0x222222), 0);
        lv_obj_add_event_cb(target_buttons[i], event_target_select,
                            LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_flag(target_buttons[i], LV_OBJ_FLAG_HIDDEN);

        target_labels[i] = lv_label_create(target_buttons[i]);
        lv_label_set_text(target_labels[i], "Player");
        lv_obj_set_style_text_color(target_labels[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(target_labels[i], &lv_font_montserrat_18, 0);
        lv_obj_center(target_labels[i]);
    }

    screen_damage_resolver = lv_obj_create(NULL);
    lv_obj_set_size(screen_damage_resolver, 360, 360);
    lv_obj_set_style_bg_color(screen_damage_resolver, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_damage_resolver, 0, 0);
    lv_obj_set_scrollbar_mode(screen_damage_resolver, LV_SCROLLBAR_MODE_OFF);

    resolver_title = lv_label_create(screen_damage_resolver);
    lv_label_set_text(resolver_title, "P1 > P2");
    lv_obj_set_style_text_color(resolver_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(resolver_title, &lv_font_montserrat_20, 0);
    lv_obj_align(resolver_title, LV_ALIGN_TOP_MID, 0, 30);

    resolver_mode_label = lv_label_create(screen_damage_resolver);
    lv_label_set_text(resolver_mode_label, "Damage");
    lv_obj_set_style_text_color(resolver_mode_label, lv_color_hex(0x8A8A8A), 0);
    lv_obj_set_style_text_font(resolver_mode_label, &lv_font_montserrat_14, 0);
    lv_obj_align(resolver_mode_label, LV_ALIGN_TOP_MID, 0, 66);

    resolver_amount = lv_label_create(screen_damage_resolver);
    lv_label_set_text(resolver_amount, "0");
    lv_obj_set_style_text_color(resolver_amount, lv_color_white(), 0);
    lv_obj_set_style_text_font(resolver_amount, &lv_font_montserrat_bold_56, 0);
    lv_obj_align(resolver_amount, LV_ALIGN_CENTER, 0, -38);

    resolver_more_button = make_button(screen_damage_resolver, "More", 100, 40,
                                       event_show_more);
    lv_obj_align(resolver_more_button, LV_ALIGN_CENTER, 0, 42);

    resolver_type_buttons[0] =
        make_type_button(screen_damage_resolver, "Damage", 24, DAMAGE_TYPE_NORMAL);
    resolver_type_buttons[1] =
        make_type_button(screen_damage_resolver, "Commander", 133, DAMAGE_TYPE_COMMANDER);
    resolver_type_buttons[2] =
        make_type_button(screen_damage_resolver, "Poison", 242, DAMAGE_TYPE_POISON);

    lv_obj_t *apply_btn = make_button(screen_damage_resolver, "Apply", 126, 46,
                                      event_apply_damage);
    lv_obj_align(apply_btn, LV_ALIGN_BOTTOM_MID, 0, -30);

    refresh_damage_resolver_ui();
}
