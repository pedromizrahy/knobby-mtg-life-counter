#include "intro.h"
#include "pregame.h"
#include "dice_art.h"

#define INTRO_FRAME_MS 55U
#define INTRO_FRAMES 42U

lv_obj_t *screen_intro=NULL;
static lv_obj_t *coin_clip=NULL,*coin_image=NULL,*heading=NULL,*subtitle=NULL,*accent=NULL;
static lv_timer_t *intro_timer=NULL;
static unsigned intro_frame=0;

void refresh_intro_ui(void)
{
    if(!screen_intro || !coin_clip) return;
    if(intro_frame < 8U) {
        /* Reveal the medallion over the opening quarter second. */
        lv_obj_set_width(coin_clip,236);
        lv_img_set_zoom(coin_image,(uint16_t)(240U+intro_frame*16U));
    } else if(intro_frame < 29U) {
        unsigned local=intro_frame-8U;
        unsigned half=local%7U;
        int w=236-(int)((half<=3U?half:7U-half)*66U);
        lv_obj_set_width(coin_clip,w);
        if(dial_art_get(0) && dial_art_get(1))
            lv_img_set_src(coin_image,dial_art_get((local/7U)&1U));
        lv_img_set_zoom(coin_image,360);
    } else {
        lv_obj_set_width(coin_clip,236);
        if(dial_art_get(0)) lv_img_set_src(coin_image,dial_art_get(0));
        lv_img_set_zoom(coin_image,360);
    }
    lv_obj_align(coin_image,LV_ALIGN_CENTER,0,0);
    if(intro_frame>=22U) {
        lv_obj_clear_flag(heading,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(subtitle,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(accent,LV_OBJ_FLAG_HIDDEN);
    }
}
static void intro_tick(lv_timer_t *t)
{
    intro_frame++;
    refresh_intro_ui();
    if(intro_frame>=INTRO_FRAMES) {
        lv_timer_pause(t);
        open_pregame_home();
    }
}
void build_intro_screen(void)
{
    const lv_img_dsc_t *heads;
    screen_intro=lv_obj_create(NULL);
    lv_obj_set_size(screen_intro,360,360);
    lv_obj_set_style_bg_color(screen_intro,lv_color_black(),0);
    lv_obj_set_style_border_width(screen_intro,0,0);
    lv_obj_set_style_pad_all(screen_intro,0,0);
    lv_obj_set_scrollbar_mode(screen_intro,LV_SCROLLBAR_MODE_OFF);

    coin_clip=lv_obj_create(screen_intro);
    lv_obj_set_size(coin_clip,236,236);
    lv_obj_set_style_border_width(coin_clip,0,0);
    lv_obj_set_style_bg_opa(coin_clip,LV_OPA_TRANSP,0);
    lv_obj_set_style_pad_all(coin_clip,0,0);
    lv_obj_set_scrollbar_mode(coin_clip,LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(coin_clip,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(coin_clip,LV_ALIGN_CENTER,0,-32);

    coin_image=lv_img_create(coin_clip);
    heads=dial_art_get(0);
    if(heads) {
        lv_img_set_src(coin_image,heads);
        lv_img_set_zoom(coin_image,360);
        lv_obj_center(coin_image);
    } else {
        lv_obj_add_flag(coin_clip,LV_OBJ_FLAG_HIDDEN);
    }

    heading=lv_label_create(screen_intro);
    lv_label_set_text(heading,"DIAL");
    lv_obj_set_style_text_font(heading,&lv_font_montserrat_32,0);
    lv_obj_set_style_text_color(heading,lv_color_hex(0xF6CF72),0);
    lv_obj_align(heading,LV_ALIGN_BOTTOM_MID,0,-77);
    lv_obj_add_flag(heading,LV_OBJ_FLAG_HIDDEN);

    accent=lv_obj_create(screen_intro);
    lv_obj_set_size(accent,108,3);
    lv_obj_set_style_border_width(accent,0,0);
    lv_obj_set_style_bg_color(accent,lv_color_hex(0xE4AE39),0);
    lv_obj_align(accent,LV_ALIGN_BOTTOM_MID,0,-67);
    lv_obj_add_flag(accent,LV_OBJ_FLAG_HIDDEN);

    subtitle=lv_label_create(screen_intro);
    lv_label_set_text(subtitle,"D O S   P R I M O S");
    lv_obj_set_style_text_font(subtitle,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(subtitle,lv_color_hex(0xEFE5CE),0);
    lv_obj_align(subtitle,LV_ALIGN_BOTTOM_MID,0,-37);
    lv_obj_add_flag(subtitle,LV_OBJ_FLAG_HIDDEN);
}
void knob_intro_init(void)
{
    intro_frame=0;
    lv_obj_add_flag(heading,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(subtitle,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(accent,LV_OBJ_FLAG_HIDDEN);
    refresh_intro_ui();
    if(!intro_timer) intro_timer=lv_timer_create(intro_tick,INTRO_FRAME_MS,NULL);
    else {
        lv_timer_set_period(intro_timer,INTRO_FRAME_MS);
        lv_timer_reset(intro_timer);
        lv_timer_resume(intro_timer);
    }
}
