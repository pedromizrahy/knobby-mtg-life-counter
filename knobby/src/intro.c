#include "intro.h"
#include "pregame.h"
#include <stdint.h>

/* Clean 360x360 native-LVGL splash. No old coin bitmap, flip or zoom.
 * The coin-flip and dice assets are not changed. */
#define INTRO_FRAME_MS 55U
#define INTRO_FRAMES 35U

lv_obj_t *screen_intro=NULL;
static lv_obj_t *outer=NULL,*middle=NULL,*core=NULL;
static lv_obj_t *north=NULL,*south=NULL,*east=NULL,*west=NULL;
static lv_obj_t *initial=NULL,*heading=NULL,*subheading=NULL,*tagline=NULL;
static lv_timer_t *intro_timer=NULL;
static unsigned intro_frame=0;

static lv_obj_t *circle(lv_obj_t *parent,int x,int y,int w,int h,
                         uint32_t bg,uint32_t border,int border_w)
{
    lv_obj_t *o=lv_obj_create(parent);
    lv_obj_set_pos(o,x,y);
    lv_obj_set_size(o,w,h);
    lv_obj_set_style_radius(o,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(bg),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(o,border_w,0);
    lv_obj_set_style_border_color(o,lv_color_hex(border),0);
    lv_obj_set_style_pad_all(o,0,0);
    lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    return o;
}
static lv_obj_t *text(lv_obj_t *parent,const char *msg,int y,
                       const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent);
    lv_label_set_text(o,msg);
    lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_align(o,LV_ALIGN_TOP_MID,0,y);
    return o;
}
void refresh_intro_ui(void)
{
    if(!screen_intro)return;
    unsigned fade=intro_frame>15U?15U:intro_frame;
    lv_opa_t a=(lv_opa_t)(45U+(210U*fade)/15U);
    lv_obj_set_style_opa(outer,a,0);
    lv_obj_set_style_opa(middle,a,0);
    lv_obj_set_style_opa(core,a,0);
    lv_obj_set_style_opa(north,a,0);
    lv_obj_set_style_opa(south,a,0);
    lv_obj_set_style_opa(east,a,0);
    lv_obj_set_style_opa(west,a,0);
    lv_obj_set_style_opa(initial,a,0);
    unsigned t=intro_frame>24U?24U:intro_frame;
    lv_obj_set_style_opa(heading,(lv_opa_t)(t<8U?0U:(t-8U)*255U/16U),0);
    lv_obj_set_style_opa(subheading,(lv_opa_t)(t<11U?0U:(t-11U)*255U/13U),0);
    lv_obj_set_style_opa(tagline,(lv_opa_t)(t<14U?0U:(t-14U)*255U/10U),0);
}
static void tick(lv_timer_t *timer)
{
    if(lv_scr_act()!=screen_intro){lv_timer_pause(timer);return;}
    ++intro_frame;
    refresh_intro_ui();
    if(intro_frame>=INTRO_FRAMES){
        lv_timer_pause(timer);
        open_pregame_home();
    }
}
void build_intro_screen(void)
{
    if(screen_intro)return;
    screen_intro=lv_obj_create(NULL);
    lv_obj_set_size(screen_intro,360,360);
    lv_obj_set_style_pad_all(screen_intro,0,0);
    lv_obj_set_style_border_width(screen_intro,0,0);
    lv_obj_set_style_bg_color(screen_intro,lv_color_hex(0x071522),0);
    lv_obj_set_style_bg_opa(screen_intro,LV_OPA_COVER,0);
    lv_obj_clear_flag(screen_intro,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen_intro,LV_SCROLLBAR_MODE_OFF);

    outer=circle(screen_intro,83,24,194,194,0x081D2C,0xD6B16E,3);
    middle=circle(screen_intro,99,40,162,162,0x0B2A3B,0x4F9397,2);
    core=circle(screen_intro,126,67,108,108,0x164251,0xEFCB83,3);
    north=circle(screen_intro,176,29,8,8,0xFFE4A6,0xFFE4A6,0);
    south=circle(screen_intro,176,205,8,8,0xFFE4A6,0xFFE4A6,0);
    west=circle(screen_intro,88,117,8,8,0xFFE4A6,0xFFE4A6,0);
    east=circle(screen_intro,264,117,8,8,0xFFE4A6,0xFFE4A6,0);
    initial=text(screen_intro,"D",103,&lv_font_montserrat_32,0xFFE3AD);
    heading=text(screen_intro,"DIAL",238,&lv_font_montserrat_32,0xFCE5B9);
    subheading=text(screen_intro,"D O S  P R I M O S",279,
                    &lv_font_montserrat_16,0xC0E0E3);
    tagline=text(screen_intro,"MAGIC  |  DADOS  |  POKEMON",326,
                 &lv_font_montserrat_14,0x91B7BF);
    refresh_intro_ui();
}
void knob_intro_init(void)
{
    if(!screen_intro)return;
    intro_frame=0;
    refresh_intro_ui();
    if(!intro_timer)
        intro_timer=lv_timer_create(tick,INTRO_FRAME_MS,NULL);
    else {
        lv_timer_set_period(intro_timer,INTRO_FRAME_MS);
        lv_timer_reset(intro_timer);
        lv_timer_resume(intro_timer);
    }
}
