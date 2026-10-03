#include "pregame.h"
#include "storage.h"
#include "game.h"
#include "ui_mp.h"
#include "ui_1p.h"
#include "net_sync.h"
#include "playgroup_api.h"
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "commander_image_decode.h"

extern void reset_all_values(void);
extern void back_to_main(void);

lv_obj_t *screen_pregame_home = NULL;
lv_obj_t *screen_pregame_multiplayer = NULL;
lv_obj_t *screen_pregame_players = NULL;
lv_obj_t *screen_pregame_playgroup = NULL;
lv_obj_t *screen_pregame_member = NULL;
lv_obj_t *screen_pregame_roster = NULL;
lv_obj_t *screen_pregame_deck = NULL;
lv_obj_t *screen_pregame_mulligans = NULL;

static int pregame_player_count = 4;
static uint8_t mulligans[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *roster_labels[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *mulligan_buttons[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *mulligan_labels[MAX_DISPLAY_PLAYERS] = {0};
static lv_obj_t *multiplayer_status_label = NULL;
static lv_timer_t *multiplayer_status_timer = NULL;
static lv_obj_t *player_count_label = NULL;
static lv_obj_t *players_status_label = NULL;
static lv_obj_t *playgroup_name_label = NULL;
static lv_obj_t *playgroup_meta_label = NULL;
static int selected_playgroup_index = 0;
static int selected_member_index[MAX_DISPLAY_PLAYERS] = {0};
static int member_picker_seat = 0;
static int member_picker_index = 0;
static lv_obj_t *member_title_label = NULL;
static lv_obj_t *member_name_label = NULL;
static lv_obj_t *member_position_label = NULL;
static lv_obj_t *member_status_label = NULL;
static bool playgroup_roster_active = false;
static long selected_deck_id[MAX_DISPLAY_PLAYERS] = {0};
static char selected_deck_name[MAX_DISPLAY_PLAYERS][PG_DECK_NAME_LEN] = {{0}};
static int deck_picker_seat = -1;
static int deck_picker_index = 0;
static lv_obj_t *deck_title_label = NULL;
static lv_obj_t *deck_name_label = NULL;
static lv_obj_t *deck_commander_label = NULL;
static lv_obj_t *deck_position_label = NULL;
static lv_obj_t *deck_image_clip = NULL;
static lv_obj_t *deck_image = NULL;
static lv_timer_t *deck_art_timer = NULL;
static uint8_t *deck_art_data = NULL;
static uint8_t *deck_art_pixels = NULL;
static lv_img_dsc_t deck_art_dsc;

static void refresh_roster(void);
static void refresh_mulligans(void);
static void refresh_playgroup_picker(void);
static void refresh_member_picker(void);
static void refresh_deck_picker(bool schedule_art);
static void schedule_deck_art(void);

static lv_obj_t *pregame_button(lv_obj_t *parent, const char *text,
                                lv_coord_t w, lv_coord_t h,
                                lv_event_cb_t cb, lv_event_code_t event,
                                void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, h / 2, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x121820), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x4A5563), 0);
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, event, user_data);
    } else {
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0C0F13), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x28313B), 0);
    }

    {
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_center(label);
    }
    return btn;
}

static void event_local_play(lv_event_t *e)
{
    int i;
    (void)e;

    playgroup_end_session();
    playgroup_roster_active = false;
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        snprintf(player_names[i], sizeof(player_names[i]), "P%d", i + 1);
        selected_deck_id[i] = 0;
        selected_deck_name[i][0] = '\0';
    }

    /* New games always open on the Commander-table default: four seats.
       The user can adjust 1..6 with touch +/- or the physical dial. */
    pregame_player_count = 4;
    if (player_count_label != NULL)
        lv_label_set_text(player_count_label, "4");
    lv_scr_load(screen_pregame_players);
}

static void refresh_multiplayer_status(void)
{
    char buf[48];
    int status = net_sync_status();
    int code = net_sync_code();

    if (multiplayer_status_label == NULL) return;

    switch (status) {
        case NET_SYNC_JOINING:
            snprintf(buf, sizeof(buf), "Searching for a host...");
            break;
        case NET_SYNC_HOSTING:
            snprintf(buf, sizeof(buf), "Hosting  #%04d", code);
            break;
        case NET_SYNC_IN_GAME:
            snprintf(buf, sizeof(buf), "Connected  #%04d", code);
            break;
        default:
            snprintf(buf, sizeof(buf), "Host owns the game settings");
            break;
    }

    lv_label_set_text(multiplayer_status_label, buf);
}

static void multiplayer_status_timer_cb(lv_timer_t *timer)
{
    if (lv_scr_act() != screen_pregame_multiplayer) {
        lv_timer_pause(timer);
        return;
    }
    refresh_multiplayer_status();
}

static void event_multiplayer_host(lv_event_t *e)
{
    (void)e;
    net_sync_start_game();
    refresh_multiplayer_status();
}

static void event_multiplayer_join(lv_event_t *e)
{
    (void)e;
    net_sync_join_game();
    refresh_multiplayer_status();
}

static void event_multiplayer_setup(lv_event_t *e)
{
    (void)e;
    refresh_multiplayer_status();
    if (multiplayer_status_timer != NULL)
        lv_timer_resume(multiplayer_status_timer);
    lv_scr_load(screen_pregame_multiplayer);
}

static void refresh_player_count_picker(void)
{
    char buf[4];

    if (player_count_label == NULL) return;
    snprintf(buf, sizeof(buf), "%d", pregame_player_count);
    lv_label_set_text(player_count_label, buf);
}

void pregame_change_player_count(int delta)
{
    int next = pregame_player_count + delta;

    if (next < 1) next = 1;
    if (next > MAX_DISPLAY_PLAYERS) next = MAX_DISPLAY_PLAYERS;
    if (next == pregame_player_count) return;

    pregame_player_count = next;
    refresh_player_count_picker();
}

static void event_player_count_adjust(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    pregame_change_player_count(delta);
}

static void open_local_roster(void)
{
    int i;
    playgroup_roster_active = false;
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++)
        snprintf(player_names[i], sizeof(player_names[i]), "P%d", i + 1);
    refresh_roster();
    lv_scr_load(screen_pregame_roster);
}

static void event_choose_players(lv_event_t *e)
{
    (void)e;

    if (!playgroup_credentials_ready()) {
        open_local_roster();
        return;
    }

    if (players_status_label != NULL) {
        lv_label_set_text(players_status_label, "Connecting to Wi-Fi...");
        lv_refr_now(NULL);
    }

    if (!playgroup_refresh_playgroups() || playgroup_cached_playgroup_count() <= 0) {
        if (players_status_label != NULL)
            lv_label_set_text(players_status_label, "Wi-Fi/API failed - tap SELECT to retry");
        return;
    }

    if (players_status_label != NULL)
        lv_label_set_text(players_status_label, "");

    selected_playgroup_index = 0;
    refresh_playgroup_picker();
    lv_scr_load(screen_pregame_playgroup);
}

static void refresh_playgroup_picker(void)
{
    const playgroup_summary_t *pg;
    char meta[48];
    int count = playgroup_cached_playgroup_count();

    if (playgroup_name_label == NULL || playgroup_meta_label == NULL) return;
    if (count <= 0) {
        lv_label_set_text(playgroup_name_label, "No playgroups");
        lv_label_set_text(playgroup_meta_label, "");
        return;
    }

    if (selected_playgroup_index < 0) selected_playgroup_index = count - 1;
    if (selected_playgroup_index >= count) selected_playgroup_index = 0;

    pg = playgroup_cached_playgroup(selected_playgroup_index);
    if (pg == NULL) return;

    lv_label_set_text(playgroup_name_label, pg->name);
    snprintf(meta, sizeof(meta), "%d members   %d/%d",
             pg->member_count, selected_playgroup_index + 1, count);
    lv_label_set_text(playgroup_meta_label, meta);
}

void pregame_change_playgroup(int delta)
{
    int count = playgroup_cached_playgroup_count();
    if (count <= 0 || delta == 0) return;

    selected_playgroup_index += (delta < 0) ? -1 : 1;
    if (selected_playgroup_index < 0) selected_playgroup_index = count - 1;
    if (selected_playgroup_index >= count) selected_playgroup_index = 0;
    refresh_playgroup_picker();
}

static void event_playgroup_adjust(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    pregame_change_playgroup(delta);
}

static void event_playgroup_select(lv_event_t *e)
{
    const playgroup_summary_t *pg;
    int member_count;
    int i;
    (void)e;

    pg = playgroup_cached_playgroup(selected_playgroup_index);
    if (pg == NULL) return;

    if (playgroup_meta_label != NULL) {
        lv_label_set_text(playgroup_meta_label, "Loading members...");
        lv_refr_now(NULL);
    }

    if (!playgroup_refresh_members(pg->id)) {
        if (playgroup_meta_label != NULL)
            lv_label_set_text(playgroup_meta_label, "Connection failed - tap SELECT to retry");
        return;
    }

    member_count = playgroup_cached_member_count();
    if (member_count <= 0) {
        if (playgroup_meta_label != NULL)
            lv_label_set_text(playgroup_meta_label, "No members found");
        return;
    }

    playgroup_roster_active = true;
    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        selected_member_index[i] = 0;
        selected_deck_id[i] = 0;
        selected_deck_name[i][0] = '\0';
    }

    member_picker_seat = 0;
    member_picker_index = 0;
    refresh_member_picker();
    lv_scr_load(screen_pregame_member);
}

static void event_roster_member_cycle(lv_event_t *e)
{
    int seat = (int)(intptr_t)lv_event_get_user_data(e);
    int member_count = playgroup_cached_member_count();

    if (!playgroup_roster_active || seat < 0 || seat >= pregame_player_count ||
        member_count <= 0)
        return;

    selected_member_index[seat] = (selected_member_index[seat] + 1) % member_count;
    selected_deck_id[seat] = 0;
    selected_deck_name[seat][0] = '\0';
    refresh_roster();
}

static void refresh_member_picker(void)
{
    const playgroup_member_t *member;
    int count = playgroup_cached_member_count();
    char title[32];
    char pos[24];

    if (count <= 0) return;

    if (member_picker_index < 0) member_picker_index = count - 1;
    if (member_picker_index >= count) member_picker_index = 0;

    member = playgroup_cached_member(member_picker_index);
    if (member == NULL) return;

    if (member_title_label != NULL) {
        snprintf(title, sizeof(title), "PLAYER %d OF %d",
                 member_picker_seat + 1, pregame_player_count);
        lv_label_set_text(member_title_label, title);
    }

    if (member_name_label != NULL)
        lv_label_set_text(member_name_label, member->username);

    if (member_position_label != NULL) {
        snprintf(pos, sizeof(pos), "%d / %d", member_picker_index + 1, count);
        lv_label_set_text(member_position_label, pos);
    }

    if (member_status_label != NULL)
        lv_label_set_text(member_status_label, "Turn dial to choose player");
}

void pregame_change_member(int delta)
{
    int count = playgroup_cached_member_count();

    if (count <= 0 || delta == 0 || lv_scr_act() != screen_pregame_member)
        return;

    member_picker_index += (delta < 0) ? -1 : 1;
    if (member_picker_index < 0) member_picker_index = count - 1;
    if (member_picker_index >= count) member_picker_index = 0;
    refresh_member_picker();
}

static void event_member_adjust(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    pregame_change_member(delta);
}

static void event_member_select(lv_event_t *e)
{
    const playgroup_member_t *member;
    char title[64];
    (void)e;

    if (member_picker_seat < 0 || member_picker_seat >= pregame_player_count)
        return;

    member = playgroup_cached_member(member_picker_index);
    if (member == NULL)
        return;

    selected_member_index[member_picker_seat] = member_picker_index;
    selected_deck_id[member_picker_seat] = 0;
    selected_deck_name[member_picker_seat][0] = '\0';
    snprintf(player_names[member_picker_seat], sizeof(player_names[member_picker_seat]),
             "%s", member->username);

    if (member_status_label != NULL) {
        lv_label_set_text(member_status_label, "Loading decks...");
        lv_refr_now(NULL);
    }

    deck_picker_seat = member_picker_seat;
    deck_picker_index = 0;

    if (!playgroup_refresh_decks(member->user_id)) {
        if (member_status_label != NULL)
            lv_label_set_text(member_status_label, "Deck load failed - tap SELECT to retry");
        return;
    }

    if (playgroup_cached_deck_count() <= 0) {
        if (member_status_label != NULL)
            lv_label_set_text(member_status_label, "No decks found for this player");
        return;
    }

    /* Start downloading this player's commander art immediately, before the
       deck picker needs it. This runs on a low-priority background task. */
    playgroup_prefetch_deck_images();

    if (deck_title_label != NULL) {
        snprintf(title, sizeof(title), "P%d  %s",
                 member_picker_seat + 1, member->username);
        lv_label_set_text(deck_title_label, title);
    }

    refresh_deck_picker(false);
    lv_scr_load(screen_pregame_deck);
    schedule_deck_art();
}

static void clear_deck_art(void)
{
    if (deck_image_clip != NULL)
        lv_obj_add_flag(deck_image_clip, LV_OBJ_FLAG_HIDDEN);

    if (deck_art_data != NULL) {
        playgroup_free_image(deck_art_data);
        deck_art_data = NULL;
    }

    if (deck_art_pixels != NULL) {
        commander_image_free_pixels(deck_art_pixels);
        deck_art_pixels = NULL;
    }

    memset(&deck_art_dsc, 0, sizeof(deck_art_dsc));
}

static void deck_art_timer_cb(lv_timer_t *timer)
{
    const playgroup_deck_t *deck;
    uint8_t *data = NULL;
    size_t data_size = 0;
    uint16_t zoom = 256;

    lv_timer_pause(timer);

    if (lv_scr_act() != screen_pregame_deck)
        return;

    deck = playgroup_cached_deck(deck_picker_index);
    if (deck == NULL || deck->art_crop_url[0] == '\0')
        return;

    if (deck_commander_label != NULL) {
        lv_label_set_text(deck_commander_label, "Loading commander art...");
        lv_refr_now(NULL);
    }

    if (!playgroup_cached_image_copy(deck->scryfall_id, &data, &data_size)) {
        /* Prefetch owns network I/O. Never block LVGL waiting for HTTPS. */
        if (deck_commander_label != NULL)
            lv_label_set_text(deck_commander_label, "Loading commander art...");
        lv_timer_set_period(timer, 350);
        lv_timer_resume(timer);
        return;
    }

    clear_deck_art();
    deck_art_data = data;

    {
        uint16_t decoded_w = 0;
        uint16_t decoded_h = 0;
        uint32_t decode_started = lv_tick_get();

        if (!commander_image_decode_rgb565(deck_art_data, data_size,
                                           &deck_art_pixels, &decoded_w, &decoded_h)) {
            printf("[Playgroup] Commander art stb JPEG decode failed.\n");
            return;
        }

        printf("[Playgroup] Commander art stb decode completed in %lu ms\n",
               (unsigned long)(lv_tick_get() - decode_started));

        memset(&deck_art_dsc, 0, sizeof(deck_art_dsc));
        deck_art_dsc.header.always_zero = 0;
        deck_art_dsc.header.w = decoded_w;
        deck_art_dsc.header.h = decoded_h;
        deck_art_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        deck_art_dsc.data_size = (uint32_t)decoded_w * (uint32_t)decoded_h * 2U;
        deck_art_dsc.data = deck_art_pixels;

        {
            uint32_t zoom_w = (190U * 256U) / decoded_w;
            uint32_t zoom_h = (130U * 256U) / decoded_h;
            zoom = (uint16_t)((zoom_w < zoom_h) ? zoom_w : zoom_h);
            if (zoom > 256U) zoom = 256U;
            if (zoom < 32U) zoom = 32U;
        }

        lv_img_set_src(deck_image, &deck_art_dsc);
        lv_img_set_zoom(deck_image, zoom);
        lv_obj_center(deck_image);
        if (deck_image_clip != NULL)
            lv_obj_clear_flag(deck_image_clip, LV_OBJ_FLAG_HIDDEN);
        lv_refr_now(NULL);

        printf("[Playgroup] Commander art shown %ux%u; zoom %u\n",
               (unsigned)decoded_w, (unsigned)decoded_h, (unsigned)zoom);
    }

    if (deck_commander_label != NULL) {
        char buf[96];
        if (deck->partner[0])
            snprintf(buf, sizeof(buf), "%s + %s", deck->commander, deck->partner);
        else
            snprintf(buf, sizeof(buf), "%s", deck->commander);
        lv_label_set_text(deck_commander_label, buf);
    }
}

static void schedule_deck_art(void)
{
    clear_deck_art();

    if (deck_art_timer == NULL)
        return;

    lv_timer_set_period(deck_art_timer, 120);
    lv_timer_reset(deck_art_timer);
    lv_timer_resume(deck_art_timer);
}

static void refresh_deck_picker(bool schedule_art)
{
    const playgroup_deck_t *deck;
    int count = playgroup_cached_deck_count();
    char pos[24];
    char commander[96];

    if (count <= 0) {
        if (deck_name_label != NULL) lv_label_set_text(deck_name_label, "No decks");
        if (deck_commander_label != NULL) lv_label_set_text(deck_commander_label, "");
        if (deck_position_label != NULL) lv_label_set_text(deck_position_label, "0 / 0");
        clear_deck_art();
        return;
    }

    if (deck_picker_index < 0) deck_picker_index = count - 1;
    if (deck_picker_index >= count) deck_picker_index = 0;

    deck = playgroup_cached_deck(deck_picker_index);
    if (deck == NULL) return;

    if (deck_name_label != NULL)
        lv_label_set_text(deck_name_label, deck->name);

    if (deck_commander_label != NULL) {
        if (deck->partner[0])
            snprintf(commander, sizeof(commander), "%s + %s",
                     deck->commander, deck->partner);
        else
            snprintf(commander, sizeof(commander), "%s", deck->commander);
        lv_label_set_text(deck_commander_label, commander);
    }

    if (deck_position_label != NULL) {
        snprintf(pos, sizeof(pos), "%d / %d", deck_picker_index + 1, count);
        lv_label_set_text(deck_position_label, pos);
    }

    if (schedule_art)
        schedule_deck_art();
}

void pregame_change_deck(int delta)
{
    int count = playgroup_cached_deck_count();
    if (count <= 0 || delta == 0 || lv_scr_act() != screen_pregame_deck)
        return;

    deck_picker_index += (delta < 0) ? -1 : 1;
    if (deck_picker_index < 0) deck_picker_index = count - 1;
    if (deck_picker_index >= count) deck_picker_index = 0;
    refresh_deck_picker(true);
}

static void event_deck_adjust(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    pregame_change_deck(delta);
}

static void event_roster_open_decks(lv_event_t *e)
{
    int seat = (int)(intptr_t)lv_event_get_user_data(e);
    const playgroup_member_t *member;
    char title[64];

    if (!playgroup_roster_active || seat < 0 || seat >= pregame_player_count)
        return;

    member = playgroup_cached_member(selected_member_index[seat]);
    if (member == NULL)
        return;

    deck_picker_seat = seat;
    deck_picker_index = 0;

    if (deck_title_label != NULL) {
        snprintf(title, sizeof(title), "P%d  %s", seat + 1, member->username);
        lv_label_set_text(deck_title_label, title);
    }

    if (!playgroup_refresh_decks(member->user_id)) {
        if (deck_name_label != NULL) lv_label_set_text(deck_name_label, "Could not load decks");
        if (deck_commander_label != NULL) lv_label_set_text(deck_commander_label, "");
        if (deck_position_label != NULL) lv_label_set_text(deck_position_label, "");
        clear_deck_art();
        lv_scr_load(screen_pregame_deck);
        return;
    }

    refresh_deck_picker(false);
    lv_scr_load(screen_pregame_deck);
    schedule_deck_art();
}

static void event_deck_select(lv_event_t *e)
{
    const playgroup_deck_t *deck;
    (void)e;

    if (deck_picker_seat < 0 || deck_picker_seat >= pregame_player_count)
        return;

    deck = playgroup_cached_deck(deck_picker_index);
    if (deck == NULL)
        return;

    selected_deck_id[deck_picker_seat] = deck->id;
    snprintf(selected_deck_name[deck_picker_seat],
             sizeof(selected_deck_name[deck_picker_seat]),
             "%s", deck->name);

    clear_deck_art();
    if (deck_art_timer != NULL)
        lv_timer_pause(deck_art_timer);

    if (deck_picker_seat + 1 < pregame_player_count) {
        member_picker_seat = deck_picker_seat + 1;
        member_picker_index = 0;
        refresh_member_picker();
        lv_scr_load(screen_pregame_member);
        return;
    }

    refresh_roster();
    lv_scr_load(screen_pregame_roster);
}

static void refresh_roster(void)
{
    int i;
    char buf[112];
    int member_count = playgroup_cached_member_count();
    int button_h;
    int button_w;
    int gap;
    int total_h;
    int top_y;
    const lv_font_t *font;

    switch (pregame_player_count) {
        case 1:
            button_h = 82; button_w = 258; gap = 0;
            font = &lv_font_montserrat_22;
            break;
        case 2:
            button_h = 68; button_w = 252; gap = 14;
            font = &lv_font_montserrat_16;
            break;
        case 3:
            button_h = 56; button_w = 246; gap = 10;
            font = &lv_font_montserrat_16;
            break;
        case 4:
            button_h = 46; button_w = 242; gap = 8;
            font = &lv_font_montserrat_14;
            break;
        case 5:
            button_h = 38; button_w = 236; gap = 5;
            font = &lv_font_montserrat_14;
            break;
        default:
            button_h = 33; button_w = 228; gap = 3;
            font = &lv_font_montserrat_14;
            break;
    }

    total_h = (pregame_player_count * button_h) +
              ((pregame_player_count - 1) * gap);
    top_y = 72 + ((202 - total_h) / 2);
    if (top_y < 68) top_y = 68;

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        lv_obj_t *btn;

        if (roster_labels[i] == NULL) continue;
        btn = lv_obj_get_parent(roster_labels[i]);

        if (i < pregame_player_count) {
            if (playgroup_roster_active && member_count > 0) {
                const playgroup_member_t *member =
                    playgroup_cached_member(selected_member_index[i] % member_count);
                if (member != NULL) {
                    snprintf(player_names[i], sizeof(player_names[i]), "%s", member->username);
                    if (selected_deck_id[i] != 0 && selected_deck_name[i][0] != '\0')
                        snprintf(buf, sizeof(buf), "P%d  %s\n%s",
                                 i + 1, member->username, selected_deck_name[i]);
                    else
                        snprintf(buf, sizeof(buf), "P%d  %s\nChoose deck",
                                 i + 1, member->username);
                } else {
                    snprintf(buf, sizeof(buf), "P%d  Select player", i + 1);
                }
            } else {
                snprintf(buf, sizeof(buf), "P%d  Local player", i + 1);
            }

            lv_label_set_text(roster_labels[i], buf);
            lv_label_set_long_mode(roster_labels[i], LV_LABEL_LONG_DOT);
            lv_obj_set_width(roster_labels[i], button_w - 24);
            lv_obj_set_style_text_align(roster_labels[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_font(roster_labels[i], font, 0);
            lv_obj_set_size(btn, button_w, button_h);
            lv_obj_set_style_radius(btn, button_h / 2, 0);
            lv_obj_align(btn, LV_ALIGN_TOP_MID, 0,
                         top_y + i * (button_h + gap));
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
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
    int button_h;
    int button_w;
    int gap;
    int total_h;
    int top_y;
    const lv_font_t *font;

    /* Use the vertical space we actually have. Four-player Commander is the
       common case, so it gets comfortable 44px rows; 1-3 get even larger
       touch targets, while only 5-6 compact enough to fit cleanly above
       START GAME. */
    switch (pregame_player_count) {
        case 1:
            button_h = 72; button_w = 250; gap = 0;
            font = &lv_font_montserrat_22;
            break;
        case 2:
            button_h = 60; button_w = 244; gap = 14;
            font = &lv_font_montserrat_22;
            break;
        case 3:
            button_h = 52; button_w = 236; gap = 10;
            font = &lv_font_montserrat_16;
            break;
        case 4:
            button_h = 44; button_w = 226; gap = 8;
            font = &lv_font_montserrat_16;
            break;
        case 5:
            button_h = 36; button_w = 216; gap = 5;
            font = &lv_font_montserrat_14;
            break;
        default:
            button_h = 32; button_w = 206; gap = 3;
            font = &lv_font_montserrat_14;
            break;
    }

    total_h = (pregame_player_count * button_h) +
              ((pregame_player_count - 1) * gap);
    /* Center the stack in the usable band below the hint and above START. */
    top_y = 72 + ((208 - total_h) / 2);
    if (top_y < 70) top_y = 70;

    for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
        lv_obj_t *btn = mulligan_buttons[i];
        if (btn == NULL || mulligan_labels[i] == NULL) continue;

        if (i < pregame_player_count) {
            snprintf(buf, sizeof(buf), "%s   Mulligan %u",
                     player_names[i], (unsigned)mulligans[i]);
            lv_label_set_text(mulligan_labels[i], buf);
            lv_obj_set_style_text_font(mulligan_labels[i], font, 0);
            lv_obj_set_size(btn, button_w, button_h);
            lv_obj_set_style_radius(btn, button_h / 2, 0);
            lv_obj_align(btn, LV_ALIGN_TOP_MID, 0,
                         top_y + i * (button_h + gap));
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
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

static void event_mulligan_decrement(lv_event_t *e)
{
    int player = (int)(intptr_t)lv_event_get_user_data(e);

    if (player < 0 || player >= pregame_player_count) return;
    if (mulligans[player] > 0) mulligans[player]--;
    refresh_mulligans();
}

static void event_start_game(lv_event_t *e)
{
    int i;
    (void)e;

    playgroup_end_session();

    nvs_set_num_players(pregame_player_count);
    nvs_set_players_to_track(pregame_player_count);
    settings_save();

    if (!playgroup_roster_active) {
        for (i = 0; i < pregame_player_count; i++)
            snprintf(player_names[i], sizeof(player_names[i]), "P%d", i + 1);
    }

    if (pregame_player_count > 1)
        rebuild_multiplayer_layout(pregame_player_count);
    reset_all_values();
    back_to_main();
}

void open_pregame_home(void)
{
    playgroup_end_session();
    if (screen_pregame_home != NULL)
        lv_scr_load(screen_pregame_home);
}

bool pregame_handle_back(lv_obj_t *screen)
{
    if (screen == screen_pregame_multiplayer) {
        lv_scr_load(screen_pregame_home);
        return true;
    }
    if (screen == screen_pregame_players) {
        playgroup_end_session();
        lv_scr_load(screen_pregame_home);
        return true;
    }
    if (screen == screen_pregame_playgroup) {
        lv_scr_load(screen_pregame_players);
        return true;
    }
    if (screen == screen_pregame_member) {
        lv_scr_load(screen_pregame_playgroup);
        return true;
    }
    if (screen == screen_pregame_roster) {
        lv_scr_load(playgroup_roster_active ? screen_pregame_member
                                            : screen_pregame_players);
        return true;
    }
    if (screen == screen_pregame_deck) {
        clear_deck_art();
        if (deck_art_timer != NULL)
            lv_timer_pause(deck_art_timer);
        member_picker_seat = deck_picker_seat;
        member_picker_index = selected_member_index[deck_picker_seat];
        refresh_member_picker();
        lv_scr_load(screen_pregame_member);
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
        lv_obj_align(eyebrow, LV_ALIGN_TOP_MID, 0, 48);

        lv_obj_t *title = lv_label_create(screen_pregame_home);
        lv_label_set_text(title, "TRACK NEW GAME");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 82);

        lv_obj_t *local = pregame_button(
            screen_pregame_home, "LOCAL PLAY", 176, 46,
            event_local_play, LV_EVENT_CLICKED, NULL);
        lv_obj_align(local, LV_ALIGN_CENTER, 0, 6);

        lv_obj_t *multi = pregame_button(
            screen_pregame_home, "MULTIPLAYER", 176, 46,
            event_multiplayer_setup, LV_EVENT_CLICKED, NULL);
        lv_obj_align(multi, LV_ALIGN_CENTER, 0, 66);
    }

    screen_pregame_multiplayer = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_multiplayer, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_multiplayer, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_multiplayer, 0, 0);

    {
        lv_obj_t *title = lv_label_create(screen_pregame_multiplayer);
        lv_label_set_text(title, "MULTIPLAYER");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 46);

        multiplayer_status_label = lv_label_create(screen_pregame_multiplayer);
        lv_label_set_text(multiplayer_status_label, "Host owns the game settings");
        lv_obj_set_style_text_color(multiplayer_status_label, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(multiplayer_status_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(multiplayer_status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(multiplayer_status_label, 250);
        lv_obj_align(multiplayer_status_label, LV_ALIGN_TOP_MID, 0, 82);

        lv_obj_t *host = pregame_button(
            screen_pregame_multiplayer, "HOST", 188, 46,
            event_multiplayer_host, LV_EVENT_CLICKED, NULL);
        lv_obj_align(host, LV_ALIGN_CENTER, 0, 8);

        lv_obj_t *join = pregame_button(
            screen_pregame_multiplayer, "JOIN", 188, 46,
            event_multiplayer_join, LV_EVENT_CLICKED, NULL);
        lv_obj_align(join, LV_ALIGN_CENTER, 0, 68);
    }

    screen_pregame_players = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_players, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_players, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_players, 0, 0);

    {
        lv_obj_t *title = lv_label_create(screen_pregame_players);
        lv_obj_t *hint;
        lv_obj_t *minus;
        lv_obj_t *plus;
        lv_obj_t *select;

        lv_label_set_text(title, "PLAYERS");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

        hint = lv_label_create(screen_pregame_players);
        lv_label_set_text(hint, "Turn dial or tap +/-");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
        lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 76);

        minus = pregame_button(screen_pregame_players, "-", 58, 58,
                               event_player_count_adjust, LV_EVENT_CLICKED,
                               (void *)(intptr_t)-1);
        lv_obj_align(minus, LV_ALIGN_CENTER, -92, -10);
        lv_obj_set_style_text_font(lv_obj_get_child(minus, 0),
                                   &lv_font_montserrat_32, 0);

        player_count_label = lv_label_create(screen_pregame_players);
        lv_label_set_text(player_count_label, "4");
        lv_obj_set_style_text_color(player_count_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(player_count_label,
                                   &lv_font_montserrat_bold_56, 0);
        lv_obj_align(player_count_label, LV_ALIGN_CENTER, 0, -12);

        plus = pregame_button(screen_pregame_players, "+", 58, 58,
                              event_player_count_adjust, LV_EVENT_CLICKED,
                              (void *)(intptr_t)1);
        lv_obj_align(plus, LV_ALIGN_CENTER, 92, -10);
        lv_obj_set_style_text_font(lv_obj_get_child(plus, 0),
                                   &lv_font_montserrat_32, 0);

        players_status_label = lv_label_create(screen_pregame_players);
        lv_label_set_text(players_status_label, "");
        lv_obj_set_width(players_status_label, 280);
        lv_obj_set_style_text_align(players_status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(players_status_label, lv_color_hex(0xC98F6B), 0);
        lv_obj_set_style_text_font(players_status_label, &lv_font_montserrat_14, 0);
        lv_obj_align(players_status_label, LV_ALIGN_CENTER, 0, 47);

        select = pregame_button(screen_pregame_players, "SELECT", 150, 44,
                                event_choose_players, LV_EVENT_CLICKED, NULL);
        lv_obj_align(select, LV_ALIGN_CENTER, 0, 86);
    }

    screen_pregame_playgroup = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_playgroup, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_playgroup, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_playgroup, 0, 0);

    {
        lv_obj_t *title = lv_label_create(screen_pregame_playgroup);
        lv_obj_t *hint;
        lv_obj_t *minus;
        lv_obj_t *plus;
        lv_obj_t *select;

        lv_label_set_text(title, "PLAYGROUP");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

        hint = lv_label_create(screen_pregame_playgroup);
        lv_label_set_text(hint, "Turn dial or tap +/-");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
        lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 76);

        minus = pregame_button(screen_pregame_playgroup, "-", 52, 52,
                               event_playgroup_adjust, LV_EVENT_CLICKED,
                               (void *)(intptr_t)-1);
        lv_obj_align(minus, LV_ALIGN_CENTER, -132, -10);

        plus = pregame_button(screen_pregame_playgroup, "+", 52, 52,
                              event_playgroup_adjust, LV_EVENT_CLICKED,
                              (void *)(intptr_t)1);
        lv_obj_align(plus, LV_ALIGN_CENTER, 132, -10);

        playgroup_name_label = lv_label_create(screen_pregame_playgroup);
        lv_label_set_text(playgroup_name_label, "Playgroup");
        lv_obj_set_width(playgroup_name_label, 190);
        lv_obj_set_style_text_align(playgroup_name_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(playgroup_name_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(playgroup_name_label, &lv_font_montserrat_22, 0);
        lv_obj_align(playgroup_name_label, LV_ALIGN_CENTER, 0, -18);

        playgroup_meta_label = lv_label_create(screen_pregame_playgroup);
        lv_label_set_text(playgroup_meta_label, "");
        lv_obj_set_style_text_color(playgroup_meta_label, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(playgroup_meta_label, &lv_font_montserrat_14, 0);
        lv_obj_align(playgroup_meta_label, LV_ALIGN_CENTER, 0, 18);

        select = pregame_button(screen_pregame_playgroup, "SELECT", 150, 44,
                                event_playgroup_select, LV_EVENT_CLICKED, NULL);
        lv_obj_align(select, LV_ALIGN_CENTER, 0, 82);
    }

    screen_pregame_member = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_member, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_member, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_member, 0, 0);

    {
        lv_obj_t *minus;
        lv_obj_t *plus;
        lv_obj_t *select;

        member_title_label = lv_label_create(screen_pregame_member);
        lv_label_set_text(member_title_label, "PLAYER 1 OF 4");
        lv_obj_set_style_text_color(member_title_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(member_title_label, &lv_font_montserrat_22, 0);
        lv_obj_align(member_title_label, LV_ALIGN_TOP_MID, 0, 42);

        member_status_label = lv_label_create(screen_pregame_member);
        lv_label_set_text(member_status_label, "Turn dial to choose player");
        lv_obj_set_width(member_status_label, 280);
        lv_obj_set_style_text_align(member_status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(member_status_label, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(member_status_label, &lv_font_montserrat_14, 0);
        lv_obj_align(member_status_label, LV_ALIGN_TOP_MID, 0, 78);

        minus = pregame_button(screen_pregame_member, "<", 52, 52,
                               event_member_adjust, LV_EVENT_CLICKED,
                               (void *)(intptr_t)-1);
        lv_obj_align(minus, LV_ALIGN_CENTER, -132, -8);

        plus = pregame_button(screen_pregame_member, ">", 52, 52,
                              event_member_adjust, LV_EVENT_CLICKED,
                              (void *)(intptr_t)1);
        lv_obj_align(plus, LV_ALIGN_CENTER, 132, -8);

        member_name_label = lv_label_create(screen_pregame_member);
        lv_label_set_text(member_name_label, "Player");
        lv_obj_set_width(member_name_label, 190);
        lv_label_set_long_mode(member_name_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(member_name_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(member_name_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(member_name_label, &lv_font_montserrat_22, 0);
        lv_obj_align(member_name_label, LV_ALIGN_CENTER, 0, -18);

        member_position_label = lv_label_create(screen_pregame_member);
        lv_label_set_text(member_position_label, "");
        lv_obj_set_style_text_color(member_position_label, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(member_position_label, &lv_font_montserrat_14, 0);
        lv_obj_align(member_position_label, LV_ALIGN_CENTER, 0, 22);

        select = pregame_button(screen_pregame_member, "SELECT PLAYER", 174, 44,
                                event_member_select, LV_EVENT_CLICKED, NULL);
        lv_obj_align(select, LV_ALIGN_CENTER, 0, 86);
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
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

        {
            lv_obj_t *hint = lv_label_create(screen_pregame_roster);
            lv_label_set_text(hint, "Review players and decks");
            lv_obj_set_style_text_color(hint, lv_color_hex(0x778391), 0);
            lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
            lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 44);
        }

        for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
            lv_obj_t *btn = pregame_button(screen_pregame_roster, "", 230, 34,
                                           event_roster_member_cycle,
                                           LV_EVENT_SHORT_CLICKED,
                                           (void *)(intptr_t)i);
            lv_obj_add_event_cb(btn, event_roster_open_decks,
                                LV_EVENT_LONG_PRESSED,
                                (void *)(intptr_t)i);
            lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 62 + (i * 37));
            roster_labels[i] = lv_obj_get_child(btn, 0);
            lv_obj_set_style_text_font(roster_labels[i], &lv_font_montserrat_14, 0);
        }

        lv_obj_t *next = pregame_button(screen_pregame_roster, "MULLIGANS", 142, 42,
                                        event_roster_continue, LV_EVENT_CLICKED, NULL);
        lv_obj_align(next, LV_ALIGN_BOTTOM_MID, 0, -18);
    }

    screen_pregame_deck = lv_obj_create(NULL);
    lv_obj_set_size(screen_pregame_deck, 360, 360);
    lv_obj_set_style_bg_color(screen_pregame_deck, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_pregame_deck, 0, 0);

    {
        lv_obj_t *hint;
        lv_obj_t *minus;
        lv_obj_t *plus;
        lv_obj_t *select;

        deck_title_label = lv_label_create(screen_pregame_deck);
        lv_label_set_text(deck_title_label, "SELECT DECK");
        lv_obj_set_width(deck_title_label, 260);
        lv_obj_set_style_text_align(deck_title_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(deck_title_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(deck_title_label, &lv_font_montserrat_16, 0);
        lv_obj_align(deck_title_label, LV_ALIGN_TOP_MID, 0, 22);

        hint = lv_label_create(screen_pregame_deck);
        lv_label_set_text(hint, "Turn dial to browse");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
        lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 47);

        deck_image_clip = lv_obj_create(screen_pregame_deck);
        lv_obj_remove_style_all(deck_image_clip);
        lv_obj_set_size(deck_image_clip, 130, 130);
        lv_obj_set_style_radius(deck_image_clip, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_clip_corner(deck_image_clip, true, 0);
        lv_obj_set_style_bg_opa(deck_image_clip, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(deck_image_clip, lv_color_hex(0x0C0F13), 0);
        lv_obj_set_style_border_width(deck_image_clip, 2, 0);
        lv_obj_set_style_border_color(deck_image_clip, lv_color_hex(0x4A5563), 0);
        lv_obj_clear_flag(deck_image_clip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(deck_image_clip, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(deck_image_clip, LV_ALIGN_CENTER, 0, -40);
        lv_obj_add_flag(deck_image_clip, LV_OBJ_FLAG_HIDDEN);

        deck_image = lv_img_create(deck_image_clip);
        lv_obj_clear_flag(deck_image, LV_OBJ_FLAG_CLICKABLE);

        minus = pregame_button(screen_pregame_deck, "<", 42, 42,
                               event_deck_adjust, LV_EVENT_CLICKED,
                               (void *)(intptr_t)-1);
        lv_obj_align(minus, LV_ALIGN_CENTER, -137, -35);

        plus = pregame_button(screen_pregame_deck, ">", 42, 42,
                              event_deck_adjust, LV_EVENT_CLICKED,
                              (void *)(intptr_t)1);
        lv_obj_align(plus, LV_ALIGN_CENTER, 137, -35);

        deck_name_label = lv_label_create(screen_pregame_deck);
        lv_label_set_text(deck_name_label, "Deck");
        lv_obj_set_width(deck_name_label, 270);
        lv_label_set_long_mode(deck_name_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(deck_name_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(deck_name_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(deck_name_label, &lv_font_montserrat_16, 0);
        lv_obj_align(deck_name_label, LV_ALIGN_BOTTOM_MID, 0, -88);

        deck_commander_label = lv_label_create(screen_pregame_deck);
        lv_label_set_text(deck_commander_label, "");
        lv_obj_set_width(deck_commander_label, 280);
        lv_label_set_long_mode(deck_commander_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(deck_commander_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(deck_commander_label, lv_color_hex(0x9CA8B5), 0);
        lv_obj_set_style_text_font(deck_commander_label, &lv_font_montserrat_14, 0);
        lv_obj_align(deck_commander_label, LV_ALIGN_BOTTOM_MID, 0, -62);

        deck_position_label = lv_label_create(screen_pregame_deck);
        lv_label_set_text(deck_position_label, "");
        lv_obj_set_style_text_color(deck_position_label, lv_color_hex(0x778391), 0);
        lv_obj_set_style_text_font(deck_position_label, &lv_font_montserrat_14, 0);
        lv_obj_align(deck_position_label, LV_ALIGN_BOTTOM_MID, 0, -40);

        select = pregame_button(screen_pregame_deck, "SELECT", 132, 38,
                                event_deck_select, LV_EVENT_CLICKED, NULL);
        lv_obj_align(select, LV_ALIGN_BOTTOM_MID, 0, -4);
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
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

        {
            lv_obj_t *hint = lv_label_create(screen_pregame_mulligans);
            lv_label_set_text(hint, "Tap +1   |   Hold -1");
            lv_obj_set_style_text_color(hint, lv_color_hex(0x6F7A85), 0);
            lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
            lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 50);
        }

        for (i = 0; i < MAX_DISPLAY_PLAYERS; i++) {
            lv_obj_t *btn = pregame_button(screen_pregame_mulligans, "", 206, 32,
                                           event_mulligan_cycle,
                                           LV_EVENT_SHORT_CLICKED,
                                           (void *)(intptr_t)i);
            lv_obj_add_event_cb(btn, event_mulligan_decrement,
                                LV_EVENT_LONG_PRESSED,
                                (void *)(intptr_t)i);
            mulligan_buttons[i] = btn;
            mulligan_labels[i] = lv_obj_get_child(btn, 0);
        }

        lv_obj_t *start = pregame_button(screen_pregame_mulligans, "START GAME", 146, 42,
                                         event_start_game, LV_EVENT_CLICKED, NULL);
        lv_obj_align(start, LV_ALIGN_BOTTOM_MID, 0, -18);
    }

    multiplayer_status_timer = lv_timer_create(multiplayer_status_timer_cb, 500, NULL);
    lv_timer_pause(multiplayer_status_timer);

    /* Art network I/O is prefetched in the background. This timer only
       polls the PSRAM cache and decodes when data is ready. */
    deck_art_timer = lv_timer_create(deck_art_timer_cb, 120, NULL);
    lv_timer_pause(deck_art_timer);

    refresh_roster();
    refresh_mulligans();
}
