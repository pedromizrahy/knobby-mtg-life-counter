#include "pokemon_ui.h"
#include "pokemon_tpk2_renderer.h"
#include <stdio.h>
#include <stdint.h>

/* A companion-focused 360x360 display, inspired by the round-screen approach
 * in TamaPoke. No artwork is copied or bundled: only the user-installed
 * /mons/p778.bin is displayed. No synthetic rewards or Playgroup numbers.
 * All controls operate outside Magic gameplay screens. */
lv_obj_t *screen_pokemon_companion = NULL;

static lv_obj_t *scene = NULL;
static lv_obj_t *terrain_back = NULL;
static lv_obj_t *terrain_front = NULL;
static lv_obj_t *sky_accent = NULL;
static lv_obj_t *mimikyu_image = NULL;
static lv_obj_t *mimikyu_status = NULL;
static lv_obj_t *theme_label = NULL;
static lv_obj_t *page_label = NULL;
static lv_obj_t *info_panel = NULL;
static lv_obj_t *stats_panel = NULL;
static lv_timer_t *mimikyu_timer = NULL;
static unsigned mimikyu_frame = 0;
static unsigned companion_page = 0;
static unsigned scene_theme = 0;

/* Theme is session-local for now; do not touch user NVS or Magic settings. */
typedef struct {
    const char *name;
    uint32_t sky, far, near, accent, text, panel;
} scene_palette_t;
static const scene_palette_t kScenes[] = {
    {"MEADOW", 0x9BD6E9, 0x77B98A, 0x398C65, 0xFFF0B3, 0x173B37, 0xEAF6E5},
    {"FOREST", 0x396B69, 0x37664F, 0x214A3B, 0xC7E4A3, 0xF6F4DC, 0x1D4036},
    {"MOONLIT",0x222E59, 0x373C70, 0x29294D, 0xE8DDA8, 0xF4F0E5, 0x242A49},
};
#define SCENE_COUNT (sizeof(kScenes)/sizeof(kScenes[0]))

static lv_obj_t *text_label(lv_obj_t *parent, const char *value, int y,
                             const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj,value);
    lv_obj_set_style_text_font(obj,font,0);
    lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);
    int parent_w=lv_obj_get_width(parent);
    lv_obj_set_width(obj,parent_w>20?parent_w-12:292);
    lv_obj_align(obj,LV_ALIGN_TOP_MID,0,y);
    return obj;
}
static lv_obj_t *scene_shape(lv_obj_t *parent, int w,int h,int x,int y,int radius)
{
    lv_obj_t *obj=lv_obj_create(parent);
    lv_obj_set_size(obj,w,h);
    lv_obj_set_style_radius(obj,radius,0);
    lv_obj_set_style_border_width(obj,0,0);
    lv_obj_set_style_shadow_width(obj,0,0);
    lv_obj_set_style_pad_all(obj,0,0);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(obj,LV_ALIGN_TOP_LEFT,x,y);
    return obj;
}
static lv_obj_t *soft_button(lv_obj_t *parent,const char *caption,
                              int width,int x,lv_event_cb_t callback)
{
    lv_obj_t *btn=lv_btn_create(parent);
    lv_obj_set_size(btn,width,38);
    lv_obj_align(btn,LV_ALIGN_TOP_MID,x,296);
    lv_obj_set_style_radius(btn,19,0);
    lv_obj_set_style_bg_color(btn,lv_color_hex(0xF6F1DE),0);
    lv_obj_set_style_bg_opa(btn,LV_OPA_90,0);
    lv_obj_set_style_border_color(btn,lv_color_hex(0x59716A),0);
    lv_obj_set_style_border_width(btn,1,0);
    lv_obj_set_style_shadow_width(btn,0,0);
    lv_obj_add_event_cb(btn,callback,LV_EVENT_CLICKED,NULL);
    lv_obj_t *label=lv_label_create(btn);
    lv_label_set_text(label,caption);
    lv_obj_set_style_text_color(label,lv_color_hex(0x26483F),0);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_14,0);
    lv_obj_center(label);
    return btn;
}
static void update_theme(void)
{
    const scene_palette_t *pal=&kScenes[scene_theme];
    lv_obj_set_style_bg_color(scene,lv_color_hex(pal->sky),0);
    lv_obj_set_style_bg_color(terrain_back,lv_color_hex(pal->far),0);
    lv_obj_set_style_bg_color(terrain_front,lv_color_hex(pal->near),0);
    lv_obj_set_style_bg_color(sky_accent,lv_color_hex(pal->accent),0);
    lv_label_set_text(theme_label,pal->name);
    lv_obj_set_style_text_color(theme_label,lv_color_hex(pal->text),0);
    lv_obj_set_style_bg_color(info_panel,lv_color_hex(pal->panel),0);
    lv_obj_set_style_bg_color(stats_panel,lv_color_hex(pal->panel),0);
}
static void update_page(void)
{
    bool home=companion_page==0;
    if(home) {
        lv_obj_clear_flag(info_panel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(stats_panel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mimikyu_image,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(page_label,"COMPANION");
    } else {
        lv_obj_add_flag(info_panel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(stats_panel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mimikyu_image,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(page_label,"PLAYGROUP STATS");
    }
    if(mimikyu_status) {
        if(home && !pk_tpk2_idle_frame_count())
            lv_obj_clear_flag(mimikyu_status,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(mimikyu_status,LV_OBJ_FLAG_HIDDEN);
    }
}
static void on_back(lv_event_t *event)
{
    (void)event;
    pokemon_ui_handle_back(lv_scr_act());
}
static void on_page(lv_event_t *event)
{
    (void)event;
    pokemon_ui_knob_page(1);
}
static void on_theme(lv_event_t *event)
{
    (void)event;
    scene_theme=(scene_theme+1U)%SCENE_COUNT;
    update_theme();
}
static void idle_tick(lv_timer_t *timer)
{
    (void)timer;
    if(lv_scr_act()!=screen_pokemon_companion || companion_page!=0 ||
       !mimikyu_image || pk_tpk2_idle_frame_count()==0) return;
    mimikyu_frame=(mimikyu_frame+1U)%pk_tpk2_idle_frame_count();
    lv_img_set_src(mimikyu_image,pk_tpk2_idle_frame(mimikyu_frame));
    lv_img_set_zoom(mimikyu_image,768); /* 3x: 32x48 Idle -> 96x144 */
    lv_obj_align(mimikyu_image,LV_ALIGN_CENTER,0,-7);
    lv_timer_set_period(mimikyu_timer,pk_tpk2_idle_duration_ms(mimikyu_frame));
}
void pokemon_ui_knob_page(int direction)
{
    if(lv_scr_act()!=screen_pokemon_companion || direction==0)return;
    companion_page=(companion_page+1U)%2U;
    update_page();
}
void pokemon_ui_open_companion(void)
{
    companion_page=0;
    bool ready=pk_tpk2_idle_load();
    mimikyu_frame=0;
    if(ready) {
        lv_img_set_src(mimikyu_image,pk_tpk2_idle_frame(0));
        lv_img_set_zoom(mimikyu_image,768);
        lv_obj_align(mimikyu_image,LV_ALIGN_CENTER,0,-7);
        lv_timer_set_period(mimikyu_timer,pk_tpk2_idle_duration_ms(0));
        lv_timer_reset(mimikyu_timer);
        lv_timer_resume(mimikyu_timer);
    } else lv_timer_pause(mimikyu_timer);
    update_page();
    if(screen_pokemon_companion)lv_scr_load(screen_pokemon_companion);
}
bool pokemon_ui_handle_back(lv_obj_t *screen)
{
    if(screen!=screen_pokemon_companion)return false;
    if(mimikyu_timer)lv_timer_pause(mimikyu_timer);
    extern lv_obj_t *screen_pregame_home;
    if(screen_pregame_home)lv_scr_load(screen_pregame_home);
    return true;
}
void pokemon_ui_build(void)
{
    screen_pokemon_companion=lv_obj_create(NULL);
    lv_obj_set_size(screen_pokemon_companion,360,360);
    lv_obj_set_style_pad_all(screen_pokemon_companion,0,0);
    lv_obj_set_style_border_width(screen_pokemon_companion,0,0);
    lv_obj_clear_flag(screen_pokemon_companion,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen_pokemon_companion,LV_SCROLLBAR_MODE_OFF);

    scene=scene_shape(screen_pokemon_companion,360,360,0,0,0);
    terrain_back=scene_shape(scene,420,160,-30,195,80);
    terrain_front=scene_shape(scene,420,140,-30,258,75);
    sky_accent=scene_shape(scene,34,34,268,69,17);
    lv_obj_set_style_bg_opa(scene,LV_OPA_COVER,0);

    page_label=text_label(screen_pokemon_companion,"COMPANION",24,
                          &lv_font_montserrat_16,0x183F39);
    text_label(screen_pokemon_companion,"MIMIKYU  #778",49,
               &lv_font_montserrat_22,0xFFF9E8);
    theme_label=text_label(screen_pokemon_companion,"MEADOW",77,
                           &lv_font_montserrat_14,0x173B37);

    mimikyu_image=lv_img_create(screen_pokemon_companion);
    lv_obj_align(mimikyu_image,LV_ALIGN_CENTER,0,-7);
    mimikyu_status=text_label(screen_pokemon_companion,"Sprite unavailable",165,
                              &lv_font_montserrat_14,0xF6F4DC);

    info_panel=scene_shape(screen_pokemon_companion,244,44,58,252,21);
    lv_obj_set_style_bg_opa(info_panel,LV_OPA_90,0);
    text_label(info_panel,"GHOST  /  FAIRY",5,&lv_font_montserrat_14,0x274F46);
    /* Real level/XP requires a connected profile; never invent stats. */
    lv_obj_t *sub=text_label(info_panel,"LV / XP: NOT LINKED",24,
                              &lv_font_montserrat_14,0x274F46);
    lv_obj_set_width(sub,225);

    stats_panel=scene_shape(screen_pokemon_companion,276,164,42,115,20);
    lv_obj_set_style_bg_opa(stats_panel,LV_OPA_90,0);
    text_label(stats_panel,"GAME STATISTICS",18,&lv_font_montserrat_16,0xF0E7C4);
    text_label(stats_panel,"Games   --       Wins   --",56,
               &lv_font_montserrat_14,0xE8F0E4);
    text_label(stats_panel,"Streak  --       Damage  --",85,
               &lv_font_montserrat_14,0xE8F0E4);
    text_label(stats_panel,"Playgroup not linked",120,
               &lv_font_montserrat_14,0xE8F0E4);

    soft_button(screen_pokemon_companion,"BACK",76,-94,on_back);
    soft_button(screen_pokemon_companion,"SCENE",88,0,on_theme);
    soft_button(screen_pokemon_companion,"STATS",76,94,on_page);

    mimikyu_timer=lv_timer_create(idle_tick,200,NULL);
    lv_timer_pause(mimikyu_timer);
    update_theme();
    update_page();
}
