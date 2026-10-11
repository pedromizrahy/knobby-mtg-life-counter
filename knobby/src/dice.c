#include "dice.h"
#include "game.h"
#include "esp_random.h"
#include <stdio.h>

/* Small, fully local first-pass animation: no GIF, sprites, Wi-Fi or heap
 * allocation per frame.  LVGL timer drives the animation at ~29 FPS. */
#define ROLL_TICK_MS 35U
#define ROLL_TICKS 34U
#define DICE_COUNT 8

static const int dice_sides[DICE_COUNT] = {0, 4, 6, 8, 10, 12, 20, 100};
static const char *dice_names[DICE_COUNT] = {
    "COIN", "D4", "D6", "D8", "D10", "D12", "D20", "D100"
};

lv_obj_t *screen_dice = NULL;
static lv_obj_t *label_type = NULL;
static lv_obj_t *label_result = NULL;
static lv_obj_t *label_hint = NULL;
static lv_obj_t *coin_face = NULL;
static lv_obj_t *coin_text = NULL;
static lv_timer_t *roll_timer = NULL;
static int selected_die = 0;
static int roll_target = 0;
static unsigned roll_tick = 0;
static bool rolling = false;

static int random_roll(int sides)
{
    if (sides == 0) return (int)(esp_random() & 1U);
    return (int)(esp_random() % (unsigned)sides) + 1;
}

void refresh_dice_ui(void)
{
    char result[12];
    if (label_type == NULL) return;
    lv_label_set_text(label_type, dice_names[selected_die]);
    if (selected_die == 0) {
        lv_obj_clear_flag(coin_face, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label_result, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(coin_text, dice_result < 0 ? "?" :
                          dice_result == 0 ? "HEADS" : "TAILS");
    } else {
        lv_obj_add_flag(coin_face, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(label_result, LV_OBJ_FLAG_HIDDEN);
        if (dice_result <= 0) lv_label_set_text(label_result, "--");
        else {
            snprintf(result, sizeof(result), "%d", dice_result);
            lv_label_set_text(label_result, result);
        }
    }
}

static void roll_animation_tick(lv_timer_t *timer)
{
    char result[12];
    int current;
    int width;
    (void)timer;
    if (!rolling || lv_scr_act() != screen_dice) {
        rolling = false;
        lv_timer_pause(roll_timer);
        return;
    }

    roll_tick++;
    current = roll_tick >= ROLL_TICKS ? roll_target :
              random_roll(dice_sides[selected_die]);
    if (selected_die == 0) {
        /* Shrink to edge-on, switch faces at the narrowest point, expand.
         * Four flips, with a slow final reveal. */
        unsigned phase = (roll_tick * 8U) / ROLL_TICKS;
        unsigned within = (roll_tick * 8U) % ROLL_TICKS;
        unsigned triangle = within <= ROLL_TICKS / 2U ?
                            within : ROLL_TICKS - within;
        width = 16 + (164 * (int)(ROLL_TICKS / 2U - triangle)) /
                      (int)(ROLL_TICKS / 2U);
        lv_obj_set_width(coin_face, width);
        lv_label_set_text(coin_text,
                          roll_tick == ROLL_TICKS ?
                          (roll_target == 0 ? "HEADS" : "TAILS") :
                          ((phase & 1U) ? "TAILS" : "HEADS"));
        lv_obj_center(coin_text);
    } else {
        snprintf(result, sizeof(result), "%d", current);
        lv_label_set_text(label_result, result);
        lv_obj_set_style_text_color(label_result,
            (roll_tick & 1U) ? lv_color_hex(0xF8C84E) :
                               lv_color_hex(0x06D6A0), 0);
    }
    if (roll_tick >= ROLL_TICKS) {
        rolling = false;
        dice_result = selected_die == 0 ? roll_target : current;
        lv_obj_set_width(coin_face, 180);
        lv_label_set_text(label_hint, "Tap to roll again");
        refresh_dice_ui();
        lv_timer_pause(roll_timer);
        printf("[Dice] %s result=%s%d\n", dice_names[selected_die],
               selected_die == 0 ? (dice_result ? "TAILS " : "HEADS ") : "",
               selected_die == 0 ? 0 : dice_result);
    }
}

void dice_change_selection(int delta)
{
    if (rolling || delta == 0) return;
    selected_die = (selected_die + (delta > 0 ? 1 : -1) + DICE_COUNT) % DICE_COUNT;
    dice_result = -1;
    refresh_dice_ui();
}

void dice_roll_selected(void)
{
    if (rolling || roll_timer == NULL) return;
    roll_target = random_roll(dice_sides[selected_die]);
    dice_result = -1;
    rolling = true;
    roll_tick = 0;
    lv_obj_set_width(coin_face, 180);
    lv_label_set_text(label_hint, "Rolling...");
    refresh_dice_ui();
    lv_timer_reset(roll_timer);
    lv_timer_resume(roll_timer);
}

void open_dice_screen(void)
{
    if (screen_dice == NULL) return;
    if (roll_timer != NULL) lv_timer_pause(roll_timer);
    rolling = false;
    selected_die = 0;
    dice_result = -1;
    lv_obj_set_width(coin_face, 180);
    lv_label_set_text(label_hint, "Turn dial to select - tap to roll");
    refresh_dice_ui();
    load_screen_if_needed(screen_dice);
}

static void event_dice_tap(lv_event_t *event)
{
    (void)event;
    dice_roll_selected();
}

void event_tool_dice(lv_event_t *event)
{
    (void)event;
    open_dice_screen();
}

void build_dice_screen(void)
{
    screen_dice = lv_obj_create(NULL);
    lv_obj_set_size(screen_dice, 360, 360);
    lv_obj_set_style_bg_color(screen_dice, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_dice, 0, 0);
    lv_obj_set_scrollbar_mode(screen_dice, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(screen_dice, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_dice, event_dice_tap, LV_EVENT_CLICKED, NULL);

    label_type = lv_label_create(screen_dice);
    lv_obj_set_style_text_color(label_type, lv_color_hex(0xF8C84E), 0);
    lv_obj_set_style_text_font(label_type, &lv_font_montserrat_32, 0);
    lv_obj_align(label_type, LV_ALIGN_TOP_MID, 0, 48);

    coin_face = lv_obj_create(screen_dice);
    lv_obj_set_size(coin_face, 180, 180);
    lv_obj_set_style_radius(coin_face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(coin_face, lv_color_hex(0x1265C8), 0);
    lv_obj_set_style_border_color(coin_face, lv_color_hex(0xF8C84E), 0);
    lv_obj_set_style_border_width(coin_face, 9, 0);
    lv_obj_set_style_pad_all(coin_face, 0, 0);
    lv_obj_set_scrollbar_mode(coin_face, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(coin_face, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(coin_face, LV_OBJ_FLAG_EVENT_BUBBLE);

    coin_text = lv_label_create(coin_face);
    lv_obj_set_style_text_color(coin_text, lv_color_white(), 0);
    lv_obj_set_style_text_font(coin_text, &lv_font_montserrat_22, 0);
    lv_obj_center(coin_text);

    label_result = lv_label_create(screen_dice);
    lv_obj_set_style_text_color(label_result, lv_color_hex(0x06D6A0), 0);
    lv_obj_set_style_text_font(label_result, &lv_font_montserrat_bold_116, 0);
    lv_obj_align(label_result, LV_ALIGN_CENTER, 0, 0);

    label_hint = lv_label_create(screen_dice);
    lv_obj_set_style_text_color(label_hint, lv_color_hex(0x9A9A9A), 0);
    lv_obj_set_style_text_font(label_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_hint, LV_ALIGN_BOTTOM_MID, 0, -52);

    roll_timer = lv_timer_create(roll_animation_tick, ROLL_TICK_MS, NULL);
    lv_timer_pause(roll_timer);
    dice_result = -1;
    refresh_dice_ui();
}
