#include "intro.h"

// Forward declaration
extern void back_to_main(void);

lv_obj_t *screen_intro = NULL;

static lv_obj_t *intro_title = NULL;
static lv_obj_t *intro_subtitle = NULL;
static lv_obj_t *intro_ring = NULL;
static uint8_t intro_step = 0;
static lv_timer_t *intro_timer = NULL;

void refresh_intro_ui(void)
{
    if (intro_title != NULL) {
        if (intro_step >= 1)
            lv_obj_clear_flag(intro_title, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(intro_title, LV_OBJ_FLAG_HIDDEN);
    }

    if (intro_subtitle != NULL) {
        if (intro_step >= 2)
            lv_obj_clear_flag(intro_subtitle, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(intro_subtitle, LV_OBJ_FLAG_HIDDEN);
    }

    if (intro_ring != NULL) {
        if (intro_step >= 1)
            lv_obj_clear_flag(intro_ring, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(intro_ring, LV_OBJ_FLAG_HIDDEN);
    }
}

static void intro_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (intro_step < 2) {
        intro_step++;
        refresh_intro_ui();
        return;
    }

    if (intro_timer != NULL) {
        lv_timer_pause(intro_timer);
    }
    back_to_main();
}

void build_intro_screen(void)
{
    screen_intro = lv_obj_create(NULL);
    lv_obj_set_size(screen_intro, 360, 360);
    lv_obj_set_style_bg_color(screen_intro, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_intro, 0, 0);
    lv_obj_set_scrollbar_mode(screen_intro, LV_SCROLLBAR_MODE_OFF);

    intro_ring = lv_arc_create(screen_intro);
    lv_obj_set_size(intro_ring, 220, 220);
    lv_obj_align(intro_ring, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(intro_ring, 270);
    lv_arc_set_bg_angles(intro_ring, 0, 360);
    lv_arc_set_range(intro_ring, 0, 100);
    lv_arc_set_value(intro_ring, 78);
    lv_obj_remove_style(intro_ring, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(intro_ring, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(intro_ring, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_arc_width(intro_ring, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(intro_ring, lv_color_hex(0x9C5CFF), LV_PART_INDICATOR);
    lv_obj_clear_flag(intro_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(intro_ring, LV_OBJ_FLAG_HIDDEN);

    intro_title = lv_label_create(screen_intro);
    lv_label_set_text(intro_title, "DIAL");
    lv_obj_set_style_text_color(intro_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(intro_title, &lv_font_montserrat_bold_44, 0);
    lv_obj_set_style_text_letter_space(intro_title, 6, 0);
    lv_obj_align(intro_title, LV_ALIGN_CENTER, 0, -16);
    lv_obj_add_flag(intro_title, LV_OBJ_FLAG_HIDDEN);

    intro_subtitle = lv_label_create(screen_intro);
    lv_label_set_text(intro_subtitle, "DOS PRIMOS");
    lv_obj_set_style_text_color(intro_subtitle, lv_color_hex(0xB8B8B8), 0);
    lv_obj_set_style_text_font(intro_subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_letter_space(intro_subtitle, 2, 0);
    lv_obj_align(intro_subtitle, LV_ALIGN_CENTER, 0, 28);
    lv_obj_add_flag(intro_subtitle, LV_OBJ_FLAG_HIDDEN);

    refresh_intro_ui();
}

void knob_intro_init(void)
{
    intro_step = 0;
    refresh_intro_ui();
    intro_timer = lv_timer_create(intro_timer_cb, 450, NULL);
    if (intro_timer != NULL) {
        lv_timer_ready(intro_timer);
    }
}
