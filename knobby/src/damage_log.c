#include "damage_log.h"
#include "game.h"
#include "timer.h"

// ---------- data ----------
typedef struct {
    uint32_t timestamp_ms;
    int8_t   player;       // target player index, 0..MAX_GAME_PLAYERS-1
    int8_t   source;       // source for cmd damage or counter type, -1 if N/A
    uint8_t  event_type;   // log_event_type_t
    int16_t  delta;
    uint16_t turn_number;
    uint16_t round_number;
    int8_t   turn_player;
    uint16_t action_id;
} damage_log_entry_t;

static damage_log_entry_t damage_log[DAMAGE_LOG_MAX];
static int damage_log_count = 0;
static int damage_log_head = 0;
static uint16_t next_action_id = 1;
static uint16_t active_action_id = 0;
static uint32_t undo_press_started_ms = 0;

// ---------- screen ----------
/* Labels are rendered one page at a time: a full 256-entry ring as one
   widget per entry would eat most of the 128KB LVGL heap and hard-freeze
   the device on alloc failure (LV_ASSERT_HANDLER). */
#define LOG_PAGE_SIZE 32

lv_obj_t *screen_damage_log = NULL;
static lv_obj_t *damage_log_container = NULL;
static lv_obj_t *delete_btn = NULL;
static lv_obj_t *page_label = NULL;
static int damage_log_selected = -1;  // index into the log, 0 = newest
static int damage_log_page = 0;       // rendered page, derived from selection
static lv_style_t log_label_style;    // shared by all entry labels

// ---------- log operations ----------
void damage_log_begin_action(void)
{
    if (active_action_id != 0) return;
    active_action_id = next_action_id++;
    if (next_action_id == 0) next_action_id = 1;
}

void damage_log_end_action(void)
{
    active_action_id = 0;
}

void damage_log_add(int player, int delta, uint8_t event_type, int source)
{
    if (delta == 0) return;
    damage_log[damage_log_head].timestamp_ms = lv_tick_get();
    damage_log[damage_log_head].player     = (int8_t)player;
    damage_log[damage_log_head].source     = (int8_t)source;
    damage_log[damage_log_head].event_type = event_type;
    damage_log[damage_log_head].delta      = (int16_t)delta;
    damage_log[damage_log_head].turn_number = (uint16_t)(turn_number > 0 ? turn_number : 0);
    damage_log[damage_log_head].round_number = (uint16_t)(round_number > 0 ? round_number : 0);
    damage_log[damage_log_head].turn_player =
        (int8_t)((active_turn_player >= 0 && active_turn_player < MAX_GAME_PLAYERS)
                     ? active_turn_player
                     : GAME_EVENT_NO_PLAYER);
    if (active_action_id != 0) {
        damage_log[damage_log_head].action_id = active_action_id;
    } else {
        damage_log[damage_log_head].action_id = next_action_id++;
        if (next_action_id == 0) next_action_id = 1;
    }
    damage_log_head = (damage_log_head + 1) % DAMAGE_LOG_MAX;
    if (damage_log_count < DAMAGE_LOG_MAX) damage_log_count++;
}

void damage_log_reset(void)
{
    damage_log_count = 0;
    damage_log_head = 0;
    active_action_id = 0;
}

/* Remove the newest entry matching player + event_type (used by elimination
   undo so the eliminating event can't also be undone from the log). */
void damage_log_remove_last_for(int player, uint8_t event_type)
{
    int i, j;
    for (i = 0; i < damage_log_count; i++) {
        int idx = (damage_log_head - 1 - i + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
        if (damage_log[idx].player == player &&
            damage_log[idx].event_type == event_type) {
            for (j = i; j > 0; j--) {
                int dst = (damage_log_head - 1 - j + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
                int src = (damage_log_head - j + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
                damage_log[dst] = damage_log[src];
            }
            damage_log_head = (damage_log_head - 1 + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
            damage_log_count--;
            return;
        }
    }
}

static void update_selection_highlight(void);
static void refresh_damage_log_ui(void);

void damage_log_select_next(void)
{
    if (damage_log_count == 0) return;
    if (damage_log_selected < damage_log_count - 1) {
        damage_log_selected++;
    }
    if (damage_log_selected / LOG_PAGE_SIZE != damage_log_page) {
        refresh_damage_log_ui();
    } else {
        update_selection_highlight();
    }
}

void damage_log_select_prev(void)
{
    if (damage_log_count == 0) return;
    if (damage_log_selected > 0) {
        damage_log_selected--;
    }
    if (damage_log_selected / LOG_PAGE_SIZE != damage_log_page) {
        refresh_damage_log_ui();
    } else {
        update_selection_highlight();
    }
}

static void undo_log_entry(const damage_log_entry_t *entry)
{
    if (entry->event_type == LOG_EVT_LIFE || entry->event_type == LOG_EVT_DAMAGE) {
        undo_life_change(entry->player, entry->delta);
    } else if (entry->event_type == LOG_EVT_CMD_DAMAGE) {
        undo_life_change(entry->player, entry->delta);
        undo_cmd_damage(entry->source, entry->player, entry->delta);
    } else if (entry->event_type == LOG_EVT_CMD_INFECT) {
        undo_counter_change(entry->player, COUNTER_TYPE_POISON, entry->delta);
        undo_cmd_damage(entry->source, entry->player, -entry->delta);
    } else if (entry->event_type == LOG_EVT_POISON) {
        undo_counter_change(entry->player, COUNTER_TYPE_POISON, entry->delta);
    } else if (entry->event_type == LOG_EVT_COUNTER) {
        undo_counter_change(entry->player, entry->source, entry->delta);
    }
}

static void remove_log_offset(int offset)
{
    int i;

    for (i = offset; i > 0; i--) {
        int dst = (damage_log_head - 1 - i + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
        int src = (damage_log_head - i + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
        damage_log[dst] = damage_log[src];
    }
    damage_log_head = (damage_log_head - 1 + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
    damage_log_count--;
}

void damage_log_undo_selected(void)
{
    int buf_idx;
    uint16_t action_id;
    int offset;

    if (damage_log_selected < 0 || damage_log_selected >= damage_log_count) return;

    buf_idx = (damage_log_head - 1 - damage_log_selected + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
    action_id = damage_log[buf_idx].action_id;

    /* Undo one logical action, newest consequence first. Multi-target,
       lifelink and All Damage entries therefore roll back atomically. */
    offset = 0;
    while (offset < damage_log_count) {
        int idx = (damage_log_head - 1 - offset + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
        if (damage_log[idx].action_id == action_id) {
            damage_log_entry_t copy = damage_log[idx];
            undo_log_entry(&copy);
            remove_log_offset(offset);
        } else {
            offset++;
        }
    }

    if (damage_log_count == 0) {
        damage_log_selected = -1;
    } else if (damage_log_selected >= damage_log_count) {
        damage_log_selected = damage_log_count - 1;
    }

    refresh_damage_log_ui();
}

void damage_log_undo_all(void)
{
    while (damage_log_count > 0) {
        damage_log_selected = 0;
        damage_log_undo_selected();
    }
}

// ---------- UI ----------
static void format_elapsed(uint32_t elapsed_s, char *out, size_t out_sz)
{
    if (elapsed_s >= 60) {
        snprintf(out, out_sz, "%2lum ago", (unsigned long)(elapsed_s / 60));
    } else {
        snprintf(out, out_sz, "%2lus ago", (unsigned long)elapsed_s);
    }
}

static void format_log_line(damage_log_entry_t *entry, char *buf, size_t buf_sz)
{
    uint32_t elapsed_s = lv_tick_elaps(entry->timestamp_ms) / 1000;
    int abs_delta = entry->delta > 0 ? entry->delta : -entry->delta;
    char time_str[16];

    buf[0] = '\0';  /* ensure a defined string if no branch below matches */
    format_elapsed(elapsed_s, time_str, sizeof(time_str));

    if (entry->event_type == LOG_EVT_DAMAGE && entry->source >= 0 &&
        entry->source < MAX_GAME_PLAYERS && entry->player >= 0 &&
        entry->player < MAX_GAME_PLAYERS) {
        snprintf(buf, buf_sz, "%s: %s dealt %d to %s",
                 time_str,
                 player_names[entry->source],
                 abs_delta,
                 player_names[entry->player]);
    } else if (entry->event_type == LOG_EVT_CMD_DAMAGE && entry->source >= 0 &&
        entry->source < MAX_GAME_PLAYERS && entry->player >= 0 &&
        entry->player < MAX_GAME_PLAYERS) {
        snprintf(buf, buf_sz, "%s: %s dealt %d cmd to %s",
                 time_str,
                 player_names[entry->source],
                 abs_delta,
                 player_names[entry->player]);
    } else if (entry->event_type == LOG_EVT_CMD_INFECT && entry->source >= 0 &&
               entry->source < MAX_GAME_PLAYERS && entry->player >= 0 &&
               entry->player < MAX_GAME_PLAYERS) {
        snprintf(buf, buf_sz, "%s: %s dealt %d cmd infect to %s",
                 time_str,
                 player_names[entry->source],
                 abs_delta,
                 player_names[entry->player]);
    } else if (entry->event_type == LOG_EVT_POISON && entry->source >= 0 &&
               entry->source < MAX_GAME_PLAYERS && entry->player >= 0 &&
               entry->player < MAX_GAME_PLAYERS) {
        snprintf(buf, buf_sz, "%s: %s gave %d poison to %s",
                 time_str,
                 player_names[entry->source],
                 abs_delta,
                 player_names[entry->player]);
    } else if (entry->event_type == LOG_EVT_COUNTER && entry->source >= 0 &&
               entry->player >= 0 && entry->player < MAX_GAME_PLAYERS) {
        const counter_definition_t *definition = get_counter_definition((counter_type_t)entry->source);
        const char *action = entry->delta > 0 ? "increased" : "decreased";
        const char *counter_name = (definition != NULL) ? definition->display_name : "Counter";
        snprintf(buf, buf_sz, "%s: %s %s %s by %d",
                 time_str,
                 player_names[entry->player],
                 action,
                 counter_name,
                 abs_delta);
    } else if (entry->player >= 0 && entry->player < MAX_GAME_PLAYERS) {
        const char *action = entry->delta > 0 ? "gained" : "lost";
        snprintf(buf, buf_sz, "%s: %s %s %d life",
                 time_str, player_names[entry->player], action, abs_delta);
    }
}

static void update_selection_highlight(void)
{
    int i;
    int first = damage_log_page * LOG_PAGE_SIZE;
    int sel_child = damage_log_selected - first;
    uint32_t child_count = lv_obj_get_child_cnt(damage_log_container);

    for (i = 0; i < (int)child_count && first + i < damage_log_count; i++) {
        lv_obj_t *lbl = lv_obj_get_child(damage_log_container, i);

        if (i == sel_child) {
            lv_obj_set_style_bg_color(lbl, lv_color_hex(0x333333), 0);
            lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
        }
    }

    /* Scroll selected item into view. Force the flex layout first: rows
       recreated this pass still have {0,0,0,0} coords until LVGL lays them
       out, which would scroll to the wrong place. */
    if (sel_child >= 0 && sel_child < (int)child_count) {
        lv_obj_t *sel = lv_obj_get_child(damage_log_container, sel_child);
        lv_obj_update_layout(damage_log_container);
        lv_coord_t sel_y = lv_obj_get_y(sel);
        lv_coord_t sel_h = lv_obj_get_height(sel);
        lv_coord_t cont_h = lv_obj_get_height(damage_log_container);
        lv_coord_t scroll_y = lv_obj_get_scroll_y(damage_log_container);

        if (sel_y - scroll_y < 0) {
            lv_obj_scroll_to_y(damage_log_container, sel_y, LV_ANIM_ON);
        } else if (sel_y + sel_h - scroll_y > cont_h) {
            lv_obj_scroll_to_y(damage_log_container, sel_y + sel_h - cont_h, LV_ANIM_ON);
        }
    }

    /* Show/hide delete button */
    if (delete_btn != NULL) {
        if (damage_log_selected >= 0) {
            lv_obj_clear_flag(delete_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(delete_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void refresh_damage_log_ui(void)
{
    int i, idx, first, last;
    char buf[128];

    lv_obj_clean(damage_log_container);

    if (damage_log_count == 0) {
        lv_obj_t *lbl = lv_label_create(damage_log_container);
        lv_label_set_text(lbl, "No events yet");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x7A7A7A), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        damage_log_selected = -1;
        damage_log_page = 0;
        if (page_label != NULL) lv_obj_add_flag(page_label, LV_OBJ_FLAG_HIDDEN);
        update_selection_highlight();
        return;
    }

    if (damage_log_selected >= damage_log_count) {
        damage_log_selected = damage_log_count - 1;
    }
    damage_log_page = (damage_log_selected > 0) ? damage_log_selected / LOG_PAGE_SIZE : 0;

    first = damage_log_page * LOG_PAGE_SIZE;
    last = first + LOG_PAGE_SIZE;
    if (last > damage_log_count) last = damage_log_count;

    for (i = first; i < last; i++) {
        idx = (damage_log_head - 1 - i + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;

        {
            char line[88];
            bool new_group = false;
            int newer_idx;

            format_log_line(&damage_log[idx], line, sizeof(line));

            if (i == first) {
                new_group = true;
            } else {
                newer_idx = (damage_log_head - i + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
                if (damage_log[newer_idx].turn_number != damage_log[idx].turn_number ||
                    damage_log[newer_idx].round_number != damage_log[idx].round_number ||
                    damage_log[newer_idx].turn_player != damage_log[idx].turn_player) {
                    new_group = true;
                }
            }

            if (new_group && damage_log[idx].turn_number > 0 &&
                damage_log[idx].turn_player >= 0 &&
                damage_log[idx].turn_player < MAX_GAME_PLAYERS) {
                snprintf(buf, sizeof(buf), "R%u · %s\n----------------\n%s",
                         (unsigned)damage_log[idx].round_number,
                         player_names[damage_log[idx].turn_player],
                         line);
            } else {
                snprintf(buf, sizeof(buf), "%s", line);
            }
        }

        lv_obj_t *lbl = lv_label_create(damage_log_container);
        lv_label_set_text(lbl, buf);
        lv_obj_set_width(lbl, 280);
        lv_obj_add_style(lbl, &log_label_style, 0);
        if (damage_log[idx].event_type == LOG_EVT_COUNTER ||
            damage_log[idx].event_type == LOG_EVT_POISON ||
            damage_log[idx].event_type == LOG_EVT_CMD_INFECT) {
            const counter_definition_t *definition =
                (damage_log[idx].event_type == LOG_EVT_POISON ||
                 damage_log[idx].event_type == LOG_EVT_CMD_INFECT)
                    ? get_counter_definition(COUNTER_TYPE_POISON)
                    : get_counter_definition((counter_type_t)damage_log[idx].source);
            lv_obj_set_style_text_color(lbl,
                definition != NULL ? lv_color_hex(definition->accent_color) : lv_color_hex(0xFFB74D), 0);
        } else {
            lv_obj_set_style_text_color(lbl,
                damage_log[idx].delta > 0 ? lv_color_hex(0x4CAF50) : lv_color_hex(0xFF5252), 0);
        }
    }

    if (page_label != NULL) {
        if (damage_log_count > LOG_PAGE_SIZE) {
            char page_buf[24];
            snprintf(page_buf, sizeof(page_buf), "%d-%d of %d", first + 1, last, damage_log_count);
            lv_label_set_text(page_label, page_buf);
            lv_obj_clear_flag(page_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(page_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_scroll_to_y(damage_log_container, 0, LV_ANIM_OFF);
    update_selection_highlight();
}

// ---------- navigation ----------
static void event_delete_pressed(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        undo_press_started_ms = lv_tick_get();
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        uint32_t held_ms = lv_tick_elaps(undo_press_started_ms);

        if (held_ms >= 3000U) {
            damage_log_undo_all();
        } else if (held_ms >= 1500U) {
            damage_log_undo_selected();
        }
        undo_press_started_ms = 0;
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        undo_press_started_ms = 0;
    }
}

void open_damage_log_screen(void)
{
    damage_log_selected = (damage_log_count > 0) ? 0 : -1;
    refresh_damage_log_ui();
    load_screen_if_needed(screen_damage_log);
}

static void event_open_damage_log(lv_event_t *e)
{
    (void)e;
    open_damage_log_screen();
}

// ---------- build ----------
void build_damage_log_screen(void)
{
    lv_obj_t *btn_label;

    screen_damage_log = lv_obj_create(NULL);
    lv_obj_set_size(screen_damage_log, 360, 360);
    lv_obj_set_style_bg_color(screen_damage_log, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_damage_log, 0, 0);
    lv_obj_set_scrollbar_mode(screen_damage_log, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(screen_damage_log);
    lv_label_set_text(title, "Event Log");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    page_label = lv_label_create(screen_damage_log);
    lv_label_set_text(page_label, "");
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(page_label, &lv_font_montserrat_14, 0);
    lv_obj_align(page_label, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_add_flag(page_label, LV_OBJ_FLAG_HIDDEN);

    lv_style_init(&log_label_style);
    lv_style_set_pad_left(&log_label_style, 4);
    lv_style_set_pad_right(&log_label_style, 4);
    lv_style_set_pad_top(&log_label_style, 2);
    lv_style_set_pad_bottom(&log_label_style, 2);
    lv_style_set_radius(&log_label_style, 4);
    lv_style_set_text_font(&log_label_style, &lv_font_montserrat_14);

    damage_log_container = lv_obj_create(screen_damage_log);
    lv_obj_remove_style_all(damage_log_container);
    lv_obj_set_size(damage_log_container, 300, 174);
    lv_obj_align(damage_log_container, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_set_flex_flow(damage_log_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(damage_log_container, 2, 0);
    lv_obj_set_scrollbar_mode(damage_log_container, LV_SCROLLBAR_MODE_OFF);

    /* Delete / Undo button */
    delete_btn = lv_btn_create(screen_damage_log);
    lv_obj_remove_style_all(delete_btn);
    lv_obj_set_size(delete_btn, 140, 48);
    lv_obj_align(delete_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_ext_click_area(delete_btn, 20);
    lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xB71C1C), 0);
    lv_obj_set_style_bg_opa(delete_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(delete_btn, 6, 0);
    lv_obj_add_event_cb(delete_btn, event_delete_pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(delete_btn, event_delete_pressed, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(delete_btn, event_delete_pressed, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_flag(delete_btn, LV_OBJ_FLAG_HIDDEN);

    btn_label = lv_label_create(delete_btn);
    lv_label_set_text(btn_label, "Hold 1.5s: Last\nHold 3s: All");
    lv_obj_set_style_text_align(btn_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_label);

}
