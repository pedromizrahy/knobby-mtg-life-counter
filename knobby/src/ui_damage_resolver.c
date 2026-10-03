#include "ui_damage_resolver.h"
#include "game.h"
#include "storage.h"
#include "ui_1p.h"
#include <string.h>

lv_obj_t *screen_damage_target = NULL;
lv_obj_t *screen_damage_resolver = NULL;

static lv_obj_t *target_buttons[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *target_labels[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *target_continue_button = NULL;

static lv_obj_t *resolver_title = NULL;
static lv_obj_t *resolver_amount = NULL;
static lv_obj_t *resolver_mode_label = NULL;
static lv_obj_t *resolver_effect_buttons[3] = {0};
static lv_obj_t *resolver_more_button = NULL;
static lv_obj_t *resolver_targets_button = NULL;

static int resolver_source = -1;
static uint8_t resolver_target_mask = 0;
static int resolver_amount_value = 0;
static bool resolver_advanced = false;
static uint8_t resolver_effects = 0;

static int target_count(void)
{
    int i;
    int count = 0;
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        if (resolver_target_mask & (1U << i)) count++;
    }
    return count;
}

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
        bool selected;

        if (player == resolver_source) continue;
        if (target_buttons[player] == NULL || target_labels[player] == NULL) continue;

        selected = (resolver_target_mask & (1U << player)) != 0;

        lv_label_set_text(target_labels[player], player_names[player]);
        lv_obj_set_pos(target_buttons[player], 70, 70 + slot * 58);
        lv_obj_clear_flag(target_buttons[player], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_width(target_buttons[player], selected ? 3 : 1, 0);
        lv_obj_set_style_border_color(target_buttons[player],
                                      selected ? lv_color_white()
                                               : lv_color_hex(0x555555), 0);
        lv_obj_set_style_bg_color(target_buttons[player],
                                  selected ? get_player_active_color(player)
                                           : lv_color_hex(0x222222), 0);

        if (player_eliminated[player]) {
            lv_obj_add_state(target_buttons[player], LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(target_buttons[player], LV_STATE_DISABLED);
        }
        slot++;
    }

    if (target_continue_button != NULL) {
        if (target_count() > 0) {
            lv_obj_clear_state(target_continue_button, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(target_continue_button, lv_color_hex(0x1565C0), 0);
        } else {
            lv_obj_add_state(target_continue_button, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(target_continue_button, lv_color_hex(0x333333), 0);
        }
    }
}

static void format_effects(char *buf, size_t buf_sz)
{
    bool first = true;

    buf[0] = '\0';
    if (resolver_effects == 0) {
        snprintf(buf, buf_sz, "Damage");
        return;
    }

    if (resolver_effects & ATTACK_EFFECT_COMMANDER) {
        snprintf(buf + strlen(buf), buf_sz - strlen(buf), "%sCommander",
                 first ? "" : " + ");
        first = false;
    }
    if (resolver_effects & ATTACK_EFFECT_INFECT) {
        snprintf(buf + strlen(buf), buf_sz - strlen(buf), "%sInfect",
                 first ? "" : " + ");
        first = false;
    }
    if (resolver_effects & ATTACK_EFFECT_LIFELINK) {
        snprintf(buf + strlen(buf), buf_sz - strlen(buf), "%sLifelink",
                 first ? "" : " + ");
    }
}

void refresh_damage_resolver_ui(void)
{
    char buf[64];
    char effects_buf[64];
    int i;
    int count = target_count();

    if (resolver_title != NULL && resolver_source >= 0) {
        if (count == 1) {
            int target;
            for (target = 0; target < MAX_DISPLAY_PLAYERS; target++) {
                if (resolver_target_mask & (1U << target)) {
                    snprintf(buf, sizeof(buf), "%s  >  %s",
                             player_names[resolver_source],
                             player_names[target]);
                    break;
                }
            }
        } else {
            snprintf(buf, sizeof(buf), "%s  >  %d targets",
                     player_names[resolver_source], count);
        }
        lv_label_set_text(resolver_title, buf);
    }

    if (resolver_amount != NULL) {
        snprintf(buf, sizeof(buf), "%d", resolver_amount_value);
        lv_label_set_text(resolver_amount, buf);
    }

    format_effects(effects_buf, sizeof(effects_buf));
    (void)effects_buf;

    for (i = 0; i < 3; i++) {
        uint8_t effect = (i == 0) ? ATTACK_EFFECT_COMMANDER :
                         (i == 1) ? ATTACK_EFFECT_INFECT :
                                    ATTACK_EFFECT_LIFELINK;
        bool selected = (resolver_effects & effect) != 0;

        if (resolver_effect_buttons[i] == NULL) continue;

        lv_obj_clear_flag(resolver_effect_buttons[i], LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_style_border_width(resolver_effect_buttons[i],
                                      selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(resolver_effect_buttons[i],
                                      selected ? lv_color_hex(0x66D9FF)
                                               : lv_color_hex(0x4A4F58), 0);
        lv_obj_set_style_bg_color(resolver_effect_buttons[i],
                                  selected ? lv_color_hex(0x16313A)
                                           : lv_color_hex(0x17191D), 0);
    }

    if (resolver_more_button != NULL)
        lv_obj_add_flag(resolver_more_button, LV_OBJ_FLAG_HIDDEN);
    if (resolver_targets_button != NULL)
        lv_obj_add_flag(resolver_targets_button, LV_OBJ_FLAG_HIDDEN);
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
    resolver_target_mask = (uint8_t)(1U << target_player);
    resolver_amount_value = 0;
    resolver_advanced = advanced;
    resolver_effects = 0;
    refresh_damage_resolver_ui();
    load_screen_if_needed(screen_damage_resolver);
}

static void event_target_toggle(lv_event_t *e)
{
    int target = (int)(intptr_t)lv_event_get_user_data(e);

    if (target < 0 || target >= MAX_DISPLAY_PLAYERS) return;
    if (target == resolver_source || player_eliminated[target]) return;

    resolver_target_mask ^= (uint8_t)(1U << target);
    refresh_target_screen();
}

static void event_target_continue(lv_event_t *e)
{
    (void)e;
    if (resolver_target_mask == 0) return;
    resolver_amount_value = 0;
    resolver_advanced = false;
    resolver_effects = 0;
    refresh_damage_resolver_ui();
    load_screen_if_needed(screen_damage_resolver);
}

static void event_effect_toggle(lv_event_t *e)
{
    uint8_t effect = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if (effect != ATTACK_EFFECT_COMMANDER &&
        effect != ATTACK_EFFECT_INFECT &&
        effect != ATTACK_EFFECT_LIFELINK) return;

    resolver_effects ^= effect;
    refresh_damage_resolver_ui();
}

static void event_show_more(lv_event_t *e)
{
    (void)e;
    resolver_advanced = true;
    refresh_damage_resolver_ui();
}

static void event_edit_targets(lv_event_t *e)
{
    (void)e;
    refresh_target_screen();
    load_screen_if_needed(screen_damage_target);
}

static void event_apply_damage(lv_event_t *e)
{
    (void)e;

    if (resolver_amount_value <= 0 || resolver_target_mask == 0) return;
    if (apply_sourced_attack(resolver_source, resolver_target_mask,
                             resolver_amount_value, resolver_effects)) {
        back_to_main();
    }
}

void open_damage_resolver(int source_player)
{
    if (source_player < 0 || source_player >= MAX_DISPLAY_PLAYERS) return;
    if (player_eliminated[source_player]) return;

    resolver_source = source_player;
    resolver_target_mask = 0;
    resolver_amount_value = 0;
    resolver_advanced = false;
    resolver_effects = 0;
    refresh_target_screen();
    load_screen_if_needed(screen_damage_target);
}

static lv_obj_t *make_effect_button(lv_obj_t *parent, const char *text,
                                    lv_coord_t x, uint8_t effect)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 78, 38);
    lv_obj_set_pos(btn, x, 194);
    lv_obj_set_style_radius(btn, 19, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x15191F), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x46505A), 0);
    lv_obj_add_event_cb(btn, event_effect_toggle, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)effect);

    {
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_center(label);
    }
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

    {
        lv_obj_t *title = lv_label_create(screen_damage_target);
        lv_label_set_text(title, "MULTIPLE TARGETS");
        lv_obj_set_style_text_color(title, lv_color_hex(0xDDE7F0), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);
    }

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        target_buttons[i] = lv_btn_create(screen_damage_target);
        lv_obj_remove_style_all(target_buttons[i]);
        lv_obj_set_size(target_buttons[i], 208, 44);
        lv_obj_set_style_radius(target_buttons[i], 22, 0);
        lv_obj_set_style_bg_color(target_buttons[i], lv_color_hex(0x171A20), 0);
        lv_obj_set_style_bg_opa(target_buttons[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(target_buttons[i], 1, 0);
        lv_obj_set_style_border_color(target_buttons[i], lv_color_hex(0x4A4F58), 0);
        lv_obj_add_event_cb(target_buttons[i], event_target_toggle,
                            LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_flag(target_buttons[i], LV_OBJ_FLAG_HIDDEN);

        target_labels[i] = lv_label_create(target_buttons[i]);
        lv_label_set_text(target_labels[i], "Player");
        lv_obj_set_style_text_color(target_labels[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(target_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_center(target_labels[i]);
    }

    target_continue_button =
        make_button(screen_damage_target, "CONTINUE", 124, 42, event_target_continue);
    lv_obj_set_style_radius(target_continue_button, 21, 0);
    lv_obj_align(target_continue_button, LV_ALIGN_BOTTOM_MID, 0, -26);

    screen_damage_resolver = lv_obj_create(NULL);
    lv_obj_set_size(screen_damage_resolver, 360, 360);
    lv_obj_set_style_bg_color(screen_damage_resolver, lv_color_hex(0x05070A), 0);
    lv_obj_set_style_border_width(screen_damage_resolver, 0, 0);
    lv_obj_set_scrollbar_mode(screen_damage_resolver, LV_SCROLLBAR_MODE_OFF);

    resolver_title = lv_label_create(screen_damage_resolver);
    lv_label_set_text(resolver_title, "P1 > P2");
    lv_obj_set_style_text_color(resolver_title, lv_color_hex(0x84909C), 0);
    lv_obj_set_style_text_font(resolver_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(resolver_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(resolver_title, LV_ALIGN_TOP_MID, 0, 42);

    resolver_mode_label = lv_label_create(screen_damage_resolver);
    lv_label_set_text(resolver_mode_label, "");
    lv_obj_add_flag(resolver_mode_label, LV_OBJ_FLAG_HIDDEN);

    resolver_amount = lv_label_create(screen_damage_resolver);
    lv_label_set_text(resolver_amount, "0");
    lv_obj_set_style_text_color(resolver_amount, lv_color_white(), 0);
    lv_obj_set_style_text_font(resolver_amount, &lv_font_montserrat_bold_56, 0);
    lv_obj_align(resolver_amount, LV_ALIGN_CENTER, 0, -48);

    /* Legacy controls intentionally not built: target editing is entered
       through the center-hold multi-target gesture, and effects are always
       visible below the amount. */
    resolver_targets_button = NULL;
    resolver_more_button = NULL;

    resolver_effect_buttons[0] =
        make_effect_button(screen_damage_resolver, "CMD", 53,
                           ATTACK_EFFECT_COMMANDER);
    resolver_effect_buttons[1] =
        make_effect_button(screen_damage_resolver, "INFECT", 141,
                           ATTACK_EFFECT_INFECT);
    resolver_effect_buttons[2] =
        make_effect_button(screen_damage_resolver, "LIFELINK", 229,
                           ATTACK_EFFECT_LIFELINK);

    {
        lv_obj_t *apply_btn =
            make_button(screen_damage_resolver, "APPLY", 112, 42,
                        event_apply_damage);
        lv_obj_remove_style_all(apply_btn);
        lv_obj_set_size(apply_btn, 112, 42);
        lv_obj_set_style_radius(apply_btn, 21, 0);
        lv_obj_set_style_bg_color(apply_btn, lv_color_hex(0x0D47A1), 0);
        lv_obj_set_style_bg_opa(apply_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(apply_btn, 1, 0);
        lv_obj_set_style_border_color(apply_btn, lv_color_hex(0x66D9FF), 0);
        {
            lv_obj_t *label = lv_obj_get_child(apply_btn, 0);
            if (label != NULL) {
                lv_obj_set_style_text_color(label, lv_color_white(), 0);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            }
        }
        lv_obj_align(apply_btn, LV_ALIGN_BOTTOM_MID, 0, -34);
    }

    refresh_damage_resolver_ui();
}
