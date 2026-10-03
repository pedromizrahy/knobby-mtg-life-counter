#include "intro.h"
#include "pregame.h"

lv_obj_t *screen_intro = NULL;

#define INTRO_TITLE_COUNT 4
#define INTRO_SUB_COUNT   10
#define INTRO_TOTAL_STEPS (INTRO_TITLE_COUNT + INTRO_SUB_COUNT)
#define INTRO_STEP_MS     280U
#define INTRO_HOLD_MS     650U

static lv_obj_t *intro_title_letters[INTRO_TITLE_COUNT] = {0};
static lv_obj_t *intro_sub_letters[INTRO_SUB_COUNT] = {0};
static uint8_t intro_step = 0;
static bool intro_holding = false;
static lv_timer_t *intro_timer = NULL;

static const char *title_text[INTRO_TITLE_COUNT] = {"D", "I", "A", "L"};
static const uint32_t title_colors[INTRO_TITLE_COUNT] = {
    0x9C5CFF, 0x42A5F5, 0x06D6A0, 0xF6C945
};
static const lv_coord_t title_x[INTRO_TITLE_COUNT] = {98, 139, 180, 221};

static const char *sub_text[INTRO_SUB_COUNT] = {
    "D", "O", "S", " ", "P", "R", "I", "M", "O", "S"
};
static const uint32_t sub_colors[INTRO_SUB_COUNT] = {
    0xB8B8B8, 0xB8B8B8, 0xB8B8B8, 0xB8B8B8,
    0x66D9FF, 0x9C5CFF, 0xF6C945, 0x06D6A0, 0x42A5F5, 0xE53935
};
static const lv_coord_t sub_x[INTRO_SUB_COUNT] = {
    72, 94, 116, 138, 156, 178, 200, 222, 246, 270
};

void refresh_intro_ui(void)
{
    int i;

    for (i = 0; i < INTRO_TITLE_COUNT; i++) {
        if (intro_title_letters[i] == NULL) continue;
        if (i < intro_step)
            lv_obj_clear_flag(intro_title_letters[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(intro_title_letters[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (i = 0; i < INTRO_SUB_COUNT; i++) {
        int visible_step = INTRO_TITLE_COUNT + i;
        if (intro_sub_letters[i] == NULL) continue;
        if (visible_step < intro_step)
            lv_obj_clear_flag(intro_sub_letters[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(intro_sub_letters[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void intro_timer_cb(lv_timer_t *timer)
{
    if (!intro_holding) {
        if (intro_step < INTRO_TOTAL_STEPS) {
            intro_step++;
            refresh_intro_ui();
            return;
        }

        intro_holding = true;
        lv_timer_set_period(timer, INTRO_HOLD_MS);
        lv_timer_reset(timer);
        return;
    }

    lv_timer_pause(timer);
    open_pregame_home();
}

void build_intro_screen(void)
{
    int i;

    screen_intro = lv_obj_create(NULL);
    lv_obj_set_size(screen_intro, 360, 360);
    lv_obj_set_style_bg_color(screen_intro, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_intro, 0, 0);
    lv_obj_set_scrollbar_mode(screen_intro, LV_SCROLLBAR_MODE_OFF);

    for (i = 0; i < INTRO_TITLE_COUNT; i++) {
        intro_title_letters[i] = lv_label_create(screen_intro);
        lv_label_set_text(intro_title_letters[i], title_text[i]);
        lv_obj_set_style_text_color(
            intro_title_letters[i], lv_color_hex(title_colors[i]), 0);
        lv_obj_set_style_text_font(
            intro_title_letters[i], &lv_font_montserrat_32, 0);
        lv_obj_set_pos(intro_title_letters[i], title_x[i], 132);
        lv_obj_add_flag(intro_title_letters[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (i = 0; i < INTRO_SUB_COUNT; i++) {
        intro_sub_letters[i] = lv_label_create(screen_intro);
        lv_label_set_text(intro_sub_letters[i], sub_text[i]);
        lv_obj_set_style_text_color(
            intro_sub_letters[i], lv_color_hex(sub_colors[i]), 0);
        lv_obj_set_style_text_font(
            intro_sub_letters[i], &lv_font_montserrat_16, 0);
        lv_obj_set_pos(intro_sub_letters[i], sub_x[i], 188);
        lv_obj_add_flag(intro_sub_letters[i], LV_OBJ_FLAG_HIDDEN);
    }

    refresh_intro_ui();
}

void knob_intro_init(void)
{
    intro_step = 0;
    intro_holding = false;
    refresh_intro_ui();

    if (intro_timer == NULL) {
        intro_timer = lv_timer_create(intro_timer_cb, INTRO_STEP_MS, NULL);
    } else {
        lv_timer_set_period(intro_timer, INTRO_STEP_MS);
        lv_timer_reset(intro_timer);
        lv_timer_resume(intro_timer);
    }
}
