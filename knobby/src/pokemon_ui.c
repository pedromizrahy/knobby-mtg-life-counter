#include "pokemon_ui.h"
#include "pokemon_tpk2_renderer.h"
#include <stdio.h>

/* Phase-one read-only prototype. Intentionally does NOT read user profiles
 * or write SPIFFS/NVS: a Playgroup account selector is not wired yet.
 * UI values below are explicitly labelled demo, not actual player rewards.
 * All Magic gameplay screens and knob life controls stay unchanged.
 */
lv_obj_t *screen_pokemon_companion = NULL;

static lv_obj_t *demo_body = NULL;
static lv_obj_t *demo_title = NULL;
static lv_obj_t *demo_line1 = NULL;
static lv_obj_t *demo_line2 = NULL;
static lv_obj_t *demo_hint = NULL;
static lv_obj_t *demo_tab = NULL;
static unsigned demo_page = 0;
static lv_obj_t *mimikyu_image = NULL;
static lv_obj_t *mimikyu_status = NULL;
static lv_timer_t *mimikyu_timer = NULL;
static unsigned mimikyu_frame = 0;

static lv_obj_t *pokemon_label(lv_obj_t *parent,const char *text,
                                int y,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *label=lv_label_create(parent);
    lv_label_set_text(label,text);
    lv_obj_set_style_text_font(label,font,0);
    lv_obj_set_style_text_color(label,lv_color_hex(color),0);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_width(label,320);
    lv_obj_align(label,LV_ALIGN_TOP_MID,0,y);
    return label;
}

static lv_obj_t *pokemon_button(lv_obj_t *parent,const char *caption,
                                 int width,int height,int x,int y,
                                 lv_event_cb_t callback)
{
    lv_obj_t *btn=lv_btn_create(parent);
    lv_obj_set_size(btn,width,height);
    lv_obj_set_style_radius(btn,14,0);
    lv_obj_set_style_bg_color(btn,lv_color_hex(0x253147),0);
    lv_obj_set_style_shadow_width(btn,0,0);
    lv_obj_set_style_border_width(btn,1,0);
    lv_obj_set_style_border_color(btn,lv_color_hex(0x475B79),0);
    lv_obj_align(btn,LV_ALIGN_TOP_MID,x,y);
    if(callback)lv_obj_add_event_cb(btn,callback,LV_EVENT_CLICKED,NULL);
    lv_obj_t *label=lv_label_create(btn);
    lv_label_set_text(label,caption);
    lv_obj_set_style_text_color(label,lv_color_white(),0);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_16,0);
    lv_obj_center(label);
    return btn;
}

static void pokemon_back_cb(lv_event_t *e)
{
    (void)e;
    pokemon_ui_handle_back(lv_scr_act());
}

static void pokemon_page_cb(lv_event_t *e)
{
    int direction=(int)(intptr_t)lv_event_get_user_data(e);
    pokemon_ui_knob_page(direction);
}

static lv_obj_t *pokemon_screen(void)
{
    lv_obj_t *screen=lv_obj_create(NULL);
    lv_obj_set_size(screen,360,360);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x0B1220),0);
    lv_obj_set_style_border_width(screen,0,0);
    lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_set_scrollbar_mode(screen,LV_SCROLLBAR_MODE_OFF);
    return screen;
}

/* Sprite pixels come solely from /mons/p778.bin; no placeholder art. */
static void pokemon_mimikyu_preview(lv_obj_t *parent)
{
    lv_obj_t *stage=lv_obj_create(parent);
    lv_obj_set_size(stage,224,156);
    lv_obj_set_style_radius(stage,18,0);
    lv_obj_set_style_bg_color(stage,lv_color_hex(0x172239),0);
    lv_obj_set_style_border_width(stage,1,0);
    lv_obj_set_style_border_color(stage,lv_color_hex(0x35435C),0);
    lv_obj_set_style_pad_all(stage,0,0);
    lv_obj_clear_flag(stage,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(stage,LV_ALIGN_TOP_MID,0,83);
    demo_body=stage;
    mimikyu_image=lv_img_create(stage);
    lv_obj_center(mimikyu_image);
    mimikyu_status=lv_label_create(stage);
    lv_label_set_text(mimikyu_status,"Sprite not loaded");
    lv_obj_set_style_text_color(mimikyu_status,lv_color_hex(0xD2DCEC),0);
    lv_obj_set_style_text_font(mimikyu_status,&lv_font_montserrat_14,0);
    lv_obj_center(mimikyu_status);
}

static void pokemon_idle_tick(lv_timer_t *timer)
{
    (void)timer;
    if(lv_scr_act()!=screen_pokemon_companion || demo_page!=0 ||
       !mimikyu_image || pk_tpk2_idle_frame_count()==0)return;
    mimikyu_frame=(mimikyu_frame+1U)%pk_tpk2_idle_frame_count();
    lv_img_set_src(mimikyu_image,pk_tpk2_idle_frame(mimikyu_frame));
    lv_obj_center(mimikyu_image);
    lv_timer_set_period(mimikyu_timer,pk_tpk2_idle_duration_ms(mimikyu_frame));
}

static void pokemon_refresh(void)
{
    if(!demo_title)return;
    static const char *titles[3]={"MIMIKYU  #778","POKE BALLS","POKEDEX"};
    static const char *lines1[3]={
        "Lv. 1   |   XP 0 / 125",
        "Poke  0   Great  0",
        "Discovered  1 / 1025"
    };
    static const char *lines2[3]={
        "Companion preview only",
        "Ultra  0   Master  0",
        "Mimikyu #778 (preview)"
    };
    static const char *hints[3]={
        "Future: feed, care and evolve",
        "Capture opens in a later update",
        "Collection unlocks in a later update"
    };
    lv_label_set_text(demo_title,titles[demo_page]);
    lv_label_set_text(demo_line1,lines1[demo_page]);
    lv_label_set_text(demo_line2,lines2[demo_page]);
    lv_label_set_text(demo_hint,hints[demo_page]);
    char tab[24];
    snprintf(tab,sizeof(tab),"%u / 3   |   DEMO",demo_page+1U);
    lv_label_set_text(demo_tab,tab);
    if(demo_body) {
        if(demo_page==0)
            lv_obj_clear_flag(demo_body,LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(demo_body,LV_OBJ_FLAG_HIDDEN);
    }
}

void pokemon_ui_knob_page(int direction)
{
    if(lv_scr_act()!=screen_pokemon_companion || direction==0)return;
    if(direction>0)demo_page=(demo_page+1U)%3U;
    else demo_page=(demo_page+2U)%3U;
    pokemon_refresh();
}

void pokemon_ui_open_companion(void)
{
    demo_page=0;
    /* The SD is touched only on explicit companion entry, never during Magic. */
    bool ready=pk_tpk2_idle_load();
    if(mimikyu_image && mimikyu_status) {
        if(ready) {
            mimikyu_frame=0;
            lv_img_set_src(mimikyu_image,pk_tpk2_idle_frame(0));
            lv_obj_center(mimikyu_image);
            lv_obj_clear_flag(mimikyu_image,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(mimikyu_status,LV_OBJ_FLAG_HIDDEN);
            if(mimikyu_timer) {
                lv_timer_set_period(mimikyu_timer,pk_tpk2_idle_duration_ms(0));
                lv_timer_reset(mimikyu_timer);
                lv_timer_resume(mimikyu_timer);
            }
        } else {
            lv_obj_add_flag(mimikyu_image,LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(mimikyu_status,"Mimikyu sprite unavailable");
            lv_obj_clear_flag(mimikyu_status,LV_OBJ_FLAG_HIDDEN);
            if(mimikyu_timer)lv_timer_pause(mimikyu_timer);
        }
    }
    pokemon_refresh();
    if(screen_pokemon_companion)lv_scr_load(screen_pokemon_companion);
}
bool pokemon_ui_handle_back(lv_obj_t *screen)
{
    if(screen==screen_pokemon_companion) {
        extern lv_obj_t *screen_pregame_home;
        if(mimikyu_timer)lv_timer_pause(mimikyu_timer);
        if(screen_pregame_home)lv_scr_load(screen_pregame_home);
        return true;
    }
    return false;
}
void pokemon_ui_build(void)
{
    screen_pokemon_companion=pokemon_screen();
    pokemon_label(screen_pokemon_companion,"POKEMON COMPANION",24,
                  &lv_font_montserrat_16,0xF6DFA9);
    pokemon_label(screen_pokemon_companion,"NO PLAYER LINKED  |  DEMO",52,
                  &lv_font_montserrat_14,0xDAAD7B);
    pokemon_mimikyu_preview(screen_pokemon_companion);
    mimikyu_timer=lv_timer_create(pokemon_idle_tick,200,NULL);
    lv_timer_pause(mimikyu_timer);
    demo_title=pokemon_label(screen_pokemon_companion,"MIMIKYU  #778",260,
                             &lv_font_montserrat_22,0xF8F1E2);
    demo_line1=pokemon_label(screen_pokemon_companion,"",283,
                             &lv_font_montserrat_16,0xD2DCEC);
    demo_line2=pokemon_label(screen_pokemon_companion,"",307,
                             &lv_font_montserrat_14,0x99AFC9);
    demo_hint=pokemon_label(screen_pokemon_companion,"",334,
                            &lv_font_montserrat_14,0x8794A9);
    demo_tab=pokemon_label(screen_pokemon_companion,"",240,
                           &lv_font_montserrat_14,0xC9A36F);

    lv_obj_t *prev=pokemon_button(screen_pokemon_companion,"<",41,36,
                                   -130,203,NULL);
    lv_obj_t *next=pokemon_button(screen_pokemon_companion,">",41,36,
                                   130,203,NULL);
    lv_obj_t *back=pokemon_button(screen_pokemon_companion,"BACK",72,32,
                                   -126,8,pokemon_back_cb);
    (void)back;
    lv_obj_add_event_cb(prev,pokemon_page_cb,LV_EVENT_CLICKED,(void *)(intptr_t)-1);
    lv_obj_add_event_cb(next,pokemon_page_cb,LV_EVENT_CLICKED,(void *)(intptr_t)1);
    pokemon_refresh();
}
