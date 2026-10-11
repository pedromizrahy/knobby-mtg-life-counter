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
    uint16_t turn_number;      /* global turn id: grouping/order */
    uint16_t round_number;
    uint16_t turn_in_round;    /* user-facing T1/T2/... within the round */
    int8_t   turn_player;
    uint32_t duration_ms;      /* populated by LOG_EVT_TURN_END */
    uint32_t turn_elapsed_ms;  /* when this action happened inside its turn */
    uint16_t action_id;
} damage_log_entry_t;

static damage_log_entry_t damage_log[DAMAGE_LOG_MAX];
static int damage_log_count = 0;
static int damage_log_head = 0;
static uint16_t next_action_id = 1;
static uint16_t active_action_id = 0;
static uint8_t active_action_depth = 0;
static uint32_t undo_press_started_ms = 0;

// ---------- screen ----------
/* Labels are rendered one page at a time: a full 256-entry ring as one
   widget per entry would eat most of the 128KB LVGL heap and hard-freeze
   the device on alloc failure (LV_ASSERT_HANDLER). */
#define LOG_PAGE_SIZE 16

lv_obj_t *screen_damage_log = NULL;
static lv_obj_t *damage_log_container = NULL;
static lv_obj_t *delete_btn = NULL;
static lv_obj_t *page_label = NULL;
static int damage_log_selected = -1;  // index into the log, 0 = newest
static int damage_log_page = 0;       // rendered page, derived from selection
static int rendered_offsets[LOG_PAGE_SIZE] = {0};
static int rendered_count = 0;
static lv_style_t log_label_style;    // shared by all entry labels

// ---------- log operations ----------
void damage_log_begin_action(void)
{
    if (active_action_depth == 0) {
        active_action_id = next_action_id++;
        if (next_action_id == 0) next_action_id = 1;
    }
    if (active_action_depth < 255) active_action_depth++;
}

void damage_log_end_action(void)
{
    if (active_action_depth == 0) return;
    active_action_depth--;
    if (active_action_depth == 0) {
        active_action_id = 0;
    }
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
    damage_log[damage_log_head].turn_in_round = (uint16_t)(turn_in_round > 0 ? turn_in_round : 0);
    damage_log[damage_log_head].duration_ms = 0;
    damage_log[damage_log_head].turn_elapsed_ms =
        (turn_number > 0 && active_turn_player >= 0)
            ? get_current_turn_elapsed_ms() : 0;
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

void damage_log_add_turn_end(int player, int round, int turn_index,
                             uint32_t duration_ms)
{
    damage_log_entry_t *entry = &damage_log[damage_log_head];

    memset(entry, 0, sizeof(*entry));
    entry->timestamp_ms = lv_tick_get();
    entry->player = (int8_t)player;
    entry->source = -1;
    entry->event_type = LOG_EVT_TURN_END;
    entry->delta = 0;
    entry->turn_number = (uint16_t)(turn_number > 0 ? turn_number : 0);
    entry->round_number = (uint16_t)(round > 0 ? round : 0);
    entry->turn_in_round = (uint16_t)(turn_index > 0 ? turn_index : 0);
    entry->turn_player = (int8_t)player;
    entry->duration_ms = duration_ms;
    entry->turn_elapsed_ms = duration_ms;
    entry->action_id = next_action_id++;
    if (next_action_id == 0) next_action_id = 1;

    damage_log_head = (damage_log_head + 1) % DAMAGE_LOG_MAX;
    if (damage_log_count < DAMAGE_LOG_MAX) damage_log_count++;
}

void damage_log_reset(void)
{
    damage_log_count = 0;
    damage_log_head = 0;
    active_action_id = 0;
    active_action_depth = 0;
}

int damage_log_record_count(void)
{
    return damage_log_count;
}

bool damage_log_record_get_oldest(int oldest_index, damage_log_record_t *out)
{
    int idx;
    const damage_log_entry_t *entry;

    if (out == NULL || oldest_index < 0 || oldest_index >= damage_log_count)
        return false;

    idx = (damage_log_head - damage_log_count + oldest_index + DAMAGE_LOG_MAX)
          % DAMAGE_LOG_MAX;
    entry = &damage_log[idx];

    out->timestamp_ms = entry->timestamp_ms;
    out->player = entry->player;
    out->source = entry->source;
    out->event_type = entry->event_type;
    out->delta = entry->delta;
    out->turn_number = entry->turn_number;
    out->round_number = entry->round_number;
    out->turn_in_round = entry->turn_in_round;
    out->turn_player = entry->turn_player;
    out->duration_ms = entry->duration_ms;
    out->turn_elapsed_ms = entry->turn_elapsed_ms;
    out->action_id = entry->action_id;
    return true;
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

static bool damage_log_offset_undoable(int offset)
{
    int idx;
    if (offset < 0 || offset >= damage_log_count) return false;
    idx = (damage_log_head - 1 - offset + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
    return damage_log[idx].event_type != LOG_EVT_TURN_END;
}

static bool same_turn(const damage_log_entry_t *a,
                      const damage_log_entry_t *b)
{
    return a->turn_number == b->turn_number &&
           a->round_number == b->round_number &&
           a->turn_player == b->turn_player;
}

/* Newest turn first; actions inside each turn read oldest -> newest. */
static int build_visual_action_offsets(int *out, int max_out)
{
    int raw = 0;
    int count = 0;

    while (raw < damage_log_count && count < max_out) {
        int group_start = raw;
        int first_idx =
            (damage_log_head - 1 - group_start + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
        int group_end = group_start + 1;
        int turn_end_offset = -1;
        int actions_added = 0;

        while (group_end < damage_log_count) {
            int idx =
                (damage_log_head - 1 - group_end + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
            if (!same_turn(&damage_log[first_idx], &damage_log[idx])) break;
            group_end++;
        }

        for (int pos = group_end - 1;
             pos >= group_start && count < max_out; pos--) {
            int idx =
                (damage_log_head - 1 - pos + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
            if (damage_log[idx].event_type == LOG_EVT_TURN_END) {
                turn_end_offset = pos;
                continue;
            }
            out[count++] = pos;
            actions_added++;
        }

        /* A completed turn is useful history even when absolutely nothing
           happened in it. Keep one TURN_END entry as a visual placeholder so
           the log still shows R/T/player/duration; it is never undoable. */
        if (actions_added == 0 && turn_end_offset >= 0 && count < max_out)
            out[count++] = turn_end_offset;

        raw = group_end;
    }
    return count;
}

static int visual_index_for_offset(const int *offsets, int count, int offset)
{
    for (int i = 0; i < count; i++)
        if (offsets[i] == offset) return i;
    return -1;
}

static int newest_undoable_offset(void)
{
    int offsets[DAMAGE_LOG_MAX];
    int count = build_visual_action_offsets(offsets, DAMAGE_LOG_MAX);

    for (int i = 0; i < count; i++)
        if (damage_log_offset_undoable(offsets[i])) return offsets[i];

    return -1;
}

void damage_log_select_next(void)
{
    int offsets[DAMAGE_LOG_MAX];
    int count = build_visual_action_offsets(offsets, DAMAGE_LOG_MAX);
    int pos;

    if (count == 0) return;
    pos = visual_index_for_offset(offsets, count, damage_log_selected);
    if (pos < 0) pos = -1;

    for (int i = pos + 1; i < count; i++) {
        if (!damage_log_offset_undoable(offsets[i])) continue;
        damage_log_selected = offsets[i];
        refresh_damage_log_ui();
        return;
    }
}

void damage_log_select_prev(void)
{
    int offsets[DAMAGE_LOG_MAX];
    int count = build_visual_action_offsets(offsets, DAMAGE_LOG_MAX);
    int pos;

    if (count == 0) return;
    pos = visual_index_for_offset(offsets, count, damage_log_selected);
    if (pos < 0) pos = count;

    for (int i = pos - 1; i >= 0; i--) {
        if (!damage_log_offset_undoable(offsets[i])) continue;
        damage_log_selected = offsets[i];
        refresh_damage_log_ui();
        return;
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

static bool damage_log_undo_selected_internal(bool refresh_ui)
{
    int buf_idx;
    uint16_t action_id;
    int offset;

    if (damage_log_selected < 0 || damage_log_selected >= damage_log_count)
        return false;

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
    } else {
        int offsets[DAMAGE_LOG_MAX];
        int count = build_visual_action_offsets(offsets, DAMAGE_LOG_MAX);
        damage_log_selected = (count > 0) ? offsets[0] : -1;
    }

    if (refresh_ui)
        refresh_damage_log_ui();

    return true;
}

void damage_log_undo_selected(void)
{
    damage_log_undo_selected_internal(true);
}

void damage_log_undo_all(void)
{
    int offset;
    bool changed = false;

    /*
     * Undo All used to rebuild the whole Event Log after every logical
     * action. Undo state first, then rebuild the visible log once.
     */
    while ((offset = newest_undoable_offset()) >= 0) {
        damage_log_selected = offset;
        if (!damage_log_undo_selected_internal(false))
            break;
        changed = true;
    }

    if (changed)
        refresh_damage_log_ui();
}

// ---------- UI ----------
static void format_turn_time(uint32_t elapsed_ms, char *out, size_t out_sz)
{
    unsigned long total_s = (unsigned long)(elapsed_ms / 1000U);
    snprintf(out, out_sz, "%lu:%02lu", total_s / 60UL, total_s % 60UL);
}

static void format_log_line(damage_log_entry_t *entry, char *buf, size_t buf_sz)
{
    int abs_delta = entry->delta > 0 ? entry->delta : -entry->delta;
    char time_str[16];

    buf[0] = '\0';
    format_turn_time(entry->turn_elapsed_ms, time_str, sizeof(time_str));

    if (entry->event_type == LOG_EVT_DAMAGE && entry->source >= 0 &&
        entry->source < MAX_GAME_PLAYERS && entry->player >= 0 &&
        entry->player < MAX_GAME_PLAYERS) {
        snprintf(buf, buf_sz, "%s  %s dealt %d to %s",
                 time_str, player_names[entry->source], abs_delta,
                 player_names[entry->player]);
    } else if (entry->event_type == LOG_EVT_CMD_DAMAGE && entry->source >= 0 &&
               entry->source < MAX_GAME_PLAYERS && entry->player >= 0 &&
               entry->player < MAX_GAME_PLAYERS) {
        snprintf(buf, buf_sz, "%s  %s dealt %d cmd to %s",
                 time_str, player_names[entry->source], abs_delta,
                 player_names[entry->player]);
    } else if (entry->event_type == LOG_EVT_CMD_INFECT && entry->source >= 0 &&
               entry->source < MAX_GAME_PLAYERS && entry->player >= 0 &&
               entry->player < MAX_GAME_PLAYERS) {
        snprintf(buf, buf_sz, "%s  %s dealt %d cmd infect to %s",
                 time_str, player_names[entry->source], abs_delta,
                 player_names[entry->player]);
    } else if (entry->event_type == LOG_EVT_POISON && entry->source >= 0 &&
               entry->source < MAX_GAME_PLAYERS && entry->player >= 0 &&
               entry->player < MAX_GAME_PLAYERS) {
        snprintf(buf, buf_sz, "%s  %s gave %d poison to %s",
                 time_str, player_names[entry->source], abs_delta,
                 player_names[entry->player]);
    } else if (entry->event_type == LOG_EVT_COUNTER && entry->source >= 0 &&
               entry->player >= 0 && entry->player < MAX_GAME_PLAYERS) {
        const counter_definition_t *definition =
            get_counter_definition((counter_type_t)entry->source);
        const char *action = entry->delta > 0 ? "increased" : "decreased";
        const char *counter_name =
            (definition != NULL) ? definition->display_name : "Counter";
        snprintf(buf, buf_sz, "%s  %s %s %s by %d",
                 time_str, player_names[entry->player], action,
                 counter_name, abs_delta);
    } else if (entry->player >= 0 && entry->player < MAX_GAME_PLAYERS) {
        const char *action = entry->delta > 0 ? "gained" : "lost";
        snprintf(buf, buf_sz, "%s  %s %s %d life",
                 time_str, player_names[entry->player], action, abs_delta);
    }
}

static void event_undo_selected_row(lv_event_t *e)
{
    int offset = (int)(intptr_t)lv_event_get_user_data(e);

    if (!damage_log_offset_undoable(offset)) return;
    damage_log_selected = offset;
    damage_log_undo_selected();
    if (lv_indev_get_act() != NULL)
        lv_indev_wait_release(lv_indev_get_act());
}

static void update_selection_highlight(void)
{
    uint32_t child_count = lv_obj_get_child_cnt(damage_log_container);

    for (int i = 0; i < (int)child_count && i < rendered_count; i++) {
        lv_obj_t *row = lv_obj_get_child(damage_log_container, i);
        bool selected = rendered_offsets[i] == damage_log_selected;

        lv_obj_set_style_bg_color(row, lv_color_hex(0x171B20), 0);
        lv_obj_set_style_bg_opa(row, selected ? LV_OPA_60 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, selected ? 3 : 0, 0);
        if (selected) {
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(0xCFEFFF), 0);
            lv_obj_update_layout(damage_log_container);
            lv_obj_scroll_to_view(row, LV_ANIM_ON);
        }
    }

    if (delete_btn != NULL) {
        if (damage_log_selected >= 0)
            lv_obj_clear_flag(delete_btn, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(delete_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void draw_undo_arrow(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_draw_line_dsc_t dsc;
    lv_point_t pts[7];

    lv_obj_get_coords(obj, &a);
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0xEAF8FF);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end = 1;

    /*
     * Familiar counter-clockwise "undo" arrow: arrowhead on the left,
     * then a rounded return path. The surrounding button border makes the
     * control read as an action, not as decoration.
     */
    pts[0] = (lv_point_t){a.x1 + 9,  a.y1 + 14};
    pts[1] = (lv_point_t){a.x1 + 14, a.y1 + 9};
    pts[2] = (lv_point_t){a.x1 + 14, a.y1 + 12};
    pts[3] = (lv_point_t){a.x1 + 20, a.y1 + 12};
    pts[4] = (lv_point_t){a.x1 + 23, a.y1 + 15};
    pts[5] = (lv_point_t){a.x1 + 23, a.y1 + 19};
    pts[6] = (lv_point_t){a.x1 + 18, a.y1 + 22};

    for (int i = 0; i < 6; i++)
        lv_draw_line(draw_ctx, &dsc, &pts[i], &pts[i + 1]);

    {
        lv_point_t wing = {a.x1 + 14, a.y1 + 19};
        lv_draw_line(draw_ctx, &dsc, &pts[0], &wing);
    }
}

static uint32_t duration_for_turn(const damage_log_entry_t *entry)
{
    uint32_t fallback = 0;

    for (int raw = 0; raw < damage_log_count; raw++) {
        int idx = (damage_log_head - 1 - raw + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
        if (!same_turn(entry, &damage_log[idx])) continue;
        if (damage_log[idx].event_type == LOG_EVT_TURN_END)
            return damage_log[idx].duration_ms;
        if (damage_log[idx].turn_elapsed_ms > fallback)
            fallback = damage_log[idx].turn_elapsed_ms;
    }

    if ((int)entry->turn_number == turn_number &&
        entry->turn_player == active_turn_player)
        return get_current_turn_elapsed_ms();

    return fallback;
}

static void refresh_damage_log_ui(void)
{
    int visual[DAMAGE_LOG_MAX];
    int visual_count = build_visual_action_offsets(visual, DAMAGE_LOG_MAX);
    int selected_visual;
    int first, last;

    lv_obj_clean(damage_log_container);
    rendered_count = 0;

    if (visual_count == 0) {
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

    selected_visual =
        visual_index_for_offset(visual, visual_count, damage_log_selected);
    if (selected_visual < 0) {
        damage_log_selected = newest_undoable_offset();
        selected_visual =
            visual_index_for_offset(visual, visual_count, damage_log_selected);
        if (selected_visual < 0) selected_visual = 0;
    }

    damage_log_page = selected_visual / LOG_PAGE_SIZE;
    first = damage_log_page * LOG_PAGE_SIZE;
    last = first + LOG_PAGE_SIZE;
    if (last > visual_count) last = visual_count;

    for (int i = first; i < last; i++) {
        int raw = visual[i];
        int idx = (damage_log_head - 1 - raw + DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX;
        damage_log_entry_t *entry = &damage_log[idx];
        bool new_group =
            (i == first) ||
            !same_turn(entry,
                &damage_log[(damage_log_head - 1 - visual[i - 1] +
                             DAMAGE_LOG_MAX) % DAMAGE_LOG_MAX]);
        char line[96];
        char header[96];
        lv_color_t event_color = lv_color_hex(0xB8B8B8);
        bool turn_only = entry->event_type == LOG_EVT_TURN_END;
        lv_obj_t *row = lv_obj_create(damage_log_container);
        int event_y = new_group ? 22 : 1;
        int event_lines = 1;
        int event_height = 18;

        rendered_offsets[rendered_count++] = raw;

        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, 280);
        lv_obj_set_height(row, turn_only ? 26 : (new_group ? 47 : 24));
        lv_obj_set_style_pad_left(row, 4, 0);
        lv_obj_set_style_pad_right(row, 4, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        if (new_group) {
            lv_obj_t *header_lbl = lv_label_create(row);
            uint32_t duration_ms = duration_for_turn(entry);
            unsigned long total_s = (unsigned long)(duration_ms / 1000U);

            if (i != first) {
                lv_obj_t *sep = lv_obj_create(row);
                lv_obj_remove_style_all(sep);
                lv_obj_set_size(sep, 272, 1);
                lv_obj_set_style_bg_color(sep, lv_color_hex(0x555555), 0);
                lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
                lv_obj_align(sep, LV_ALIGN_TOP_LEFT, 0, 0);
                lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
                event_y = 24;
            }

            snprintf(header, sizeof(header), "R%u | T%u | %s | %lu:%02lu",
                     (unsigned)entry->round_number,
                     (unsigned)(entry->turn_in_round > 0 ? entry->turn_in_round : 1),
                     (entry->turn_player >= 0 &&
                      entry->turn_player < MAX_GAME_PLAYERS)
                         ? player_names[entry->turn_player] : "P?",
                     total_s / 60UL, total_s % 60UL);
            lv_label_set_text(header_lbl, header);
            lv_obj_set_style_text_color(header_lbl, lv_color_hex(0xB0B0B0), 0);
            lv_obj_set_style_text_font(header_lbl, &lv_font_montserrat_14, 0);
            lv_obj_align(header_lbl, LV_ALIGN_TOP_LEFT, 0, (i != first) ? 2 : 0);
        }

        if (!turn_only) {
            format_log_line(entry, line, sizeof(line));
            if (entry->event_type == LOG_EVT_DAMAGE ||
                entry->event_type == LOG_EVT_CMD_DAMAGE ||
                entry->event_type == LOG_EVT_CMD_INFECT ||
                entry->event_type == LOG_EVT_POISON) {
                event_color = lv_color_hex(0xFF5252);
            } else if (entry->event_type == LOG_EVT_LIFE) {
                event_color = (entry->delta > 0)
                            ? lv_color_hex(0x4CAF50)
                            : lv_color_hex(0xFF5252);
            } else if (entry->event_type == LOG_EVT_COUNTER) {
                event_color = (entry->delta > 0)
                            ? lv_color_hex(0xFFB74D)
                            : lv_color_hex(0xB8B8B8);
            }

            lv_obj_t *event_lbl = lv_label_create(row);
            lv_coord_t text_w = raw == damage_log_selected ? 232 : 272;
            lv_point_t text_size;

            lv_label_set_text(event_lbl, line);
            lv_obj_set_style_text_color(event_lbl, event_color, 0);
            lv_obj_set_style_text_font(event_lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_width(event_lbl, text_w);

            lv_txt_get_size(&text_size, line, &lv_font_montserrat_14,
                            0, 0, text_w, LV_TEXT_FLAG_NONE);
            event_lines = (text_size.y > 20) ? 2 : 1;
            event_height = (event_lines == 2) ? 36 : 18;

            /*
             * Keep the full useful text when it fits in two lines. If a very
             * long event would need a third line, LV_LABEL_LONG_DOT clips it
             * cleanly inside the fixed two-line height instead of letting it
             * overlap the next row.
             */
            lv_obj_set_height(event_lbl, event_height);
            lv_label_set_long_mode(event_lbl, LV_LABEL_LONG_DOT);
            lv_obj_align(event_lbl, LV_ALIGN_TOP_LEFT, 0, event_y);

            /* Row height follows the rendered event, so wrapped text owns its
               vertical space and can never collide with the next action. */
            lv_obj_set_height(row, event_y + event_height + 5);
        }

        if (raw == damage_log_selected && damage_log_offset_undoable(raw)) {
            lv_obj_t *undo = lv_btn_create(row);

            lv_obj_remove_style_all(undo);
            lv_obj_set_size(undo, 34, 30);
            lv_obj_align(undo, LV_ALIGN_RIGHT_MID, -2,
                         new_group ? ((event_y - 2) / 2) : 0);
            lv_obj_set_style_radius(undo, 10, 0);
            lv_obj_set_style_bg_color(undo, lv_color_hex(0x202830), 0);
            lv_obj_set_style_bg_opa(undo, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(undo, 1, 0);
            lv_obj_set_style_border_color(undo, lv_color_hex(0xCFEFFF), 0);
            lv_obj_set_ext_click_area(undo, 8);
            lv_obj_add_event_cb(undo, draw_undo_arrow, LV_EVENT_DRAW_MAIN, NULL);
            lv_obj_add_event_cb(undo, event_undo_selected_row,
                                LV_EVENT_CLICKED, (void *)(intptr_t)raw);
        }
    }

    if (page_label != NULL) {
        if (visual_count > LOG_PAGE_SIZE) {
            char page_buf[24];
            snprintf(page_buf, sizeof(page_buf), "%d-%d of %d",
                     first + 1, last, visual_count);
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

        if (held_ms >= 2000U) {
            damage_log_undo_all();
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
    damage_log_selected = newest_undoable_offset();
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
    lv_obj_set_style_pad_row(damage_log_container, 0, 0);
    lv_obj_set_scrollbar_mode(damage_log_container, LV_SCROLLBAR_MODE_OFF);

    /* Delete / Undo button */
    delete_btn = lv_btn_create(screen_damage_log);
    lv_obj_remove_style_all(delete_btn);
    lv_obj_set_size(delete_btn, 140, 48);
    lv_obj_align(delete_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_ext_click_area(delete_btn, 20);
    lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0x141414), 0);
    lv_obj_set_style_bg_opa(delete_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(delete_btn, 2, 0);
    lv_obj_set_style_border_color(delete_btn, lv_color_hex(0x8A2D2D), 0);
    lv_obj_set_style_radius(delete_btn, 10, 0);
    lv_obj_add_event_cb(delete_btn, event_delete_pressed, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(delete_btn, event_delete_pressed, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(delete_btn, event_delete_pressed, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_flag(delete_btn, LV_OBJ_FLAG_HIDDEN);

    btn_label = lv_label_create(delete_btn);
    lv_label_set_text(btn_label, "Undo All\n(Hold 2s)");
    lv_obj_set_style_text_align(btn_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_label);

}
