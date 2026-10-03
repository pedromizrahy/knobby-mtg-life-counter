#include "intro.h"

// Forward declaration
extern void back_to_main(void);

lv_obj_t *screen_intro = NULL;

static lv_obj_t *intro_title = NULL;
static lv_obj_t *intro_subtitle = NULL;
static lv_obj_t *intro_ring = NULL;
static lv_timer_t *intro_timer = NULL;
static uint32_t intro_started_ms = 0;
static bool intro_done = false;

#define INTRO_DURATION_MS 3000U
#define INTRO_TICK_MS      50U

static lv_opa_t intro_fade(uint32_t elapsed, uint32_t start, uint32_t duration)
{
    uint32_t local;

    if (elapsed <= start) return LV_OPA_TRANSP;
    local = elapsed - start;
    if (local >= duration) return LV_OPA_COVER;

    return (lv_opa_t)((local * LV_OPA_COVER) / duration);
}

void refresh_intro_ui(void)
{
    uint32_t elapsed;
    int progress;

    if (screen_intro == NULL) return;

    elapsed = lv_tick_elaps(intro_started_ms);
    if (elapsed > INTRO_DURATION_MS) elapsed = INTRO_DURATION_MS;

    /* The ring progresses through the whole splash instead of appearing
       almost complete for a split second. */
    progress = (int)((elapsed * 100U) / INTRO_DURATION_MS);
    if (intro_ring != NULL) {
        lv_arc_set_value(intro_ring, progress);
        lv_obj_set_style_opa(intro_ring,
                             intro_fade(elapsed, 100U, 450U), 0);
    }

    if (intro_title != NULL) {
        lv_obj_set_style_opa(intro_title,
                             intro_fade(elapsed, 200U, 500U), 0);
    }

    if (intro_subtitle != NULL) {
        lv_obj_set_style_opa(intro_subtitle,
                             intro_fade(elapsed, 850U, 550U), 0);
    }
}

static void intro_timer_cb(lv_timer_t *timer)
{
    uint32_t elapsed = lv_tick_elaps(intro_started_ms);

    refresh_intro_ui();

    if (!intro_done && elapsed >= INTRO_DURATION_MS) {
        intro_done = true;
        lv_timer_pause(timer);
        back_to_main();
    }
}

void build_intro_screen(void)
{
    screen_intro = lv_obj_create(NULL);
    lv_obj_set_size(screen_intro, 360, 360);
    lv_obj_set_style_bg_color(screen_intro, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_border_width(screen_intro, 0, 0);
    lv_obj_set_scrollbar_mode(screen_intro, LV_SCROLLBAR_MODE_OFF);

    intro_ring = lv_arc_create(screen_intro);
    lv_obj_set_size(intro_ring, 224, 224);
    lv_obj_align(intro_ring, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(intro_ring, 270);
    lv_arc_set_bg_angles(intro_ring, 0, 360);
    lv_arc_set_range(intro_ring, 0, 100);
    lv_arc_set_value(intro_ring, 0);
    lv_obj_remove_style(intro_ring, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(intro_ring, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(intro_ring, lv_color_hex(0x202734), LV_PART_MAIN);
    lv_obj_set_style_arc_width(intro_ring, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(intro_ring, lv_color_hex(0x66D9FF), LV_PART_INDICATOR);
    lv_obj_clear_flag(intro_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(intro_ring, LV_OPA_TRANSP, 0);

    intro_title = lv_label_create(screen_intro);
    lv_label_set_text(intro_title, "DIAL");
    lv_obj_set_style_text_color(intro_title, lv_color_white(), 0);
    /* The custom bold-44 asset is numeric-only on the device, which rendered
       letters as missing-glyph boxes. Use the built-in Montserrat text font. */
    lv_obj_set_style_text_font(intro_title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_letter_space(intro_title, 7, 0);
    lv_obj_align(intro_title, LV_ALIGN_CENTER, 3, -16);
    lv_obj_set_style_opa(intro_title, LV_OPA_TRANSP, 0);

    intro_subtitle = lv_label_create(screen_intro);
    lv_label_set_text(intro_subtitle, "DOS PRIMOS");
    lv_obj_set_style_text_color(intro_subtitle, lv_color_hex(0xAFC2D4), 0);
    lv_obj_set_style_text_font(intro_subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_letter_space(intro_subtitle, 3, 0);
    lv_obj_align(intro_subtitle, LV_ALIGN_CENTER, 1, 28);
    lv_obj_set_style_opa(intro_subtitle, LV_OPA_TRANSP, 0);
}

void knob_intro_init(void)
{
    intro_started_ms = lv_tick_get();
    intro_done = false;

    if (intro_timer == NULL) {
        intro_timer = lv_timer_create(intro_timer_cb, INTRO_TICK_MS, NULL);
    } else {
        lv_timer_set_period(intro_timer, INTRO_TICK_MS);
        lv_timer_reset(intro_timer);
        lv_timer_resume(intro_timer);
    }

    refresh_intro_ui();
}
