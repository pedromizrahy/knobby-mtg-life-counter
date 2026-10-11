#include "intro.h"
#include "pregame.h"
#include "dice_art.h"
#include "dial_sprite.h"

#define INTRO_FRAME_MS 55U
#define INTRO_FRAMES 36U

lv_obj_t *screen_intro=NULL;
static lv_obj_t *coin_clip=NULL,*coin_image=NULL,*heading=NULL,*subtitle=NULL,*accent=NULL;
static lv_timer_t *intro_timer=NULL;
static unsigned intro_frame=0;

void refresh_intro_ui(void)
{
    if (!screen_intro || !coin_clip) return;
    /* The intro uses one pristine HEADS frame. Never resize the viewport
     * or swap to TAILS: those operations produced the black curtain. */
    lv_obj_set_width(coin_clip,280);
    lv_obj_align(coin_clip,LV_ALIGN_CENTER,0,-32);
    const lv_img_dsc_t *heads=dial_sprite_available(99) ? NULL : dial_art_get(0);
    if(dial_sprite_available(99)) {
        lv_obj_add_flag(coin_clip,LV_OBJ_FLAG_HIDDEN);
        /* Decode the full-quality static image once, not on every intro tick. */
        if(intro_frame==0U) dial_sprite_show(screen_intro,99,0U,0);
        dial_sprite_intro_anim(intro_frame);
    } else if(heads) {
        lv_img_set_src(coin_image,heads);
        lv_obj_clear_flag(coin_clip,LV_OBJ_FLAG_HIDDEN);
        /* LVGL zoom of 256 means native image quality; no repeated resample. */
        uint32_t natural=heads->header.w;
        uint16_t zoom=(uint16_t)((256U*270U)/(natural ? natural : 1U));
        if(zoom>256U) zoom=256U; 
        lv_img_set_zoom(coin_image,zoom);
        lv_obj_center(coin_image);
    }
    if(intro_frame>=11U) {
        lv_obj_clear_flag(heading,LV_OBJ_FLAG_HIDDEN);
    }
    if(intro_frame>=17U) {
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
    lv_obj_set_size(coin_clip,280,280);
    lv_obj_set_style_border_width(coin_clip,0,0);
    lv_obj_set_style_bg_opa(coin_clip,LV_OPA_TRANSP,0);
    lv_obj_set_style_pad_all(coin_clip,0,0);
    lv_obj_set_scrollbar_mode(coin_clip,LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(coin_clip,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(coin_clip,LV_ALIGN_CENTER,0,-32);

    coin_image=lv_img_create(coin_clip);
    heads=dial_sprite_available(99) ? NULL : dial_art_get(0);
    if(heads) {
        lv_img_set_src(coin_image,heads);
        lv_img_set_zoom(coin_image,256);
        lv_obj_center(coin_image);
    } else {
        lv_obj_add_flag(coin_clip,LV_OBJ_FLAG_HIDDEN);
    }

    heading=lv_label_create(screen_intro);
    lv_label_set_text(heading,"DIAL DOS");
    lv_obj_set_style_text_font(heading,&lv_font_montserrat_32,0);
    lv_obj_set_style_text_color(heading,lv_color_hex(0xFFE09C),0);
    lv_obj_align(heading,LV_ALIGN_TOP_MID,0,266);
    lv_obj_add_flag(heading,LV_OBJ_FLAG_HIDDEN);

    accent=lv_obj_create(screen_intro);
    lv_obj_set_size(accent,108,3);
    lv_obj_set_style_border_width(accent,0,0);
    lv_obj_set_style_bg_color(accent,lv_color_hex(0xE4AE39),0);
    lv_obj_align(accent,LV_ALIGN_TOP_MID,0,307);
    lv_obj_add_flag(accent,LV_OBJ_FLAG_HIDDEN);

    subtitle=lv_label_create(screen_intro);
    lv_label_set_text(subtitle,"P R I M O S");
    lv_obj_set_style_text_font(subtitle,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(subtitle,lv_color_hex(0xEFE5CE),0);
    lv_obj_align(subtitle,LV_ALIGN_TOP_MID,0,316);
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
