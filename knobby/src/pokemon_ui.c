#include "pokemon_ui.h"
#include <stdio.h>

/* Phase-one read-only prototype. Intentionally does NOT read user profiles
 * or write SPIFFS/NVS: a Playgroup account selector is not wired yet.
 * UI values below are explicitly labelled demo, not actual player rewards.
 * All Magic gameplay screens and knob life controls stay unchanged.
 */
lv_obj_t *screen_pokemon_extras = NULL;
lv_obj_t *screen_pokemon_companion = NULL;

static lv_obj_t *demo_body = NULL;
static lv_obj_t *demo_title = NULL;
static lv_obj_t *demo_line1 = NULL;
static lv_obj_t *demo_line2 = NULL;
static lv_obj_t *demo_hint = NULL;
static lv_obj_t *demo_tab = NULL;
static unsigned demo_page = 0;

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

static void pokemon_open_cb(lv_event_t *e)
{
    (void)e;
    pokemon_ui_open_companion();
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

static lv_obj_t *mascot_shape(lv_obj_t *parent,int width,int height,
                              int x,int y,int radius,uint32_t color)
{
    lv_obj_t *obj=lv_obj_create(parent);
    lv_obj_set_size(obj,width,height);
    lv_obj_set_style_radius(obj,radius,0);
    lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(obj,0,0);
    lv_obj_set_style_shadow_width(obj,0,0);
    lv_obj_set_style_pad_all(obj,0,0);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(obj,LV_ALIGN_TOP_MID,x,y);
    return obj;
}

static void pokemon_mimikyu_preview(lv_obj_t *parent)
{
    /* Simple LVGL-only mascot placeholder until licensed/local sprite
     * assets are integrated. Do not claim this is an official sprite. */
    lv_obj_t *stage=lv_obj_create(parent);
    lv_obj_set_size(stage,224,176);
    lv_obj_set_style_radius(stage,18,0);
    lv_obj_set_style_bg_color(stage,lv_color_hex(0x172239),0);
    lv_obj_set_style_border_width(stage,1,0);
    lv_obj_set_style_border_color(stage,lv_color_hex(0x35435C),0);
    lv_obj_set_style_pad_all(stage,0,0);
    lv_obj_clear_flag(stage,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(stage,LV_ALIGN_TOP_MID,0,83);
    demo_body=stage;

    mascot_shape(stage,23,79,-49,11,10,0xE4D7B4);
    mascot_shape(stage,23,79,49,11,10,0xE4D7B4);
    mascot_shape(stage,130,107,0,55,52,0xE4D7B4);
    mascot_shape(stage,17,18,-27,89,9,0x22202D);
    mascot_shape(stage,17,18,27,89,9,0x22202D);
    mascot_shape(stage,30,4,0,124,3,0x7F686D);
    mascot_shape(stage,10,6,-40,120,3,0xC19A97);
    mascot_shape(stage,10,6,40,120,3,0xC19A97);
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

void pokemon_ui_open_extras(void)
{
    if(screen_pokemon_extras)lv_scr_load(screen_pokemon_extras);
}
void pokemon_ui_open_companion(void)
{
    demo_page=0;
    pokemon_refresh();
    if(screen_pokemon_companion)lv_scr_load(screen_pokemon_companion);
}
bool pokemon_ui_handle_back(lv_obj_t *screen)
{
    if(screen==screen_pokemon_companion) {
        lv_scr_load(screen_pokemon_extras);
        return true;
    }
    if(screen==screen_pokemon_extras) {
        extern lv_obj_t *screen_pregame_home;
        lv_scr_load(screen_pregame_home);
        return true;
    }
    return false;
}
void pokemon_ui_build(void)
{
    screen_pokemon_extras=pokemon_screen();
    pokemon_label(screen_pokemon_extras,"EXTRAS",44,
                  &lv_font_montserrat_22,0xF6DFA9);
    pokemon_label(screen_pokemon_extras,"Outside Magic matches",85,
                  &lv_font_montserrat_14,0x8E9FB8);
    pokemon_button(screen_pokemon_extras,"POKEMON",222,68,0,139,
                   pokemon_open_cb);
    pokemon_label(screen_pokemon_extras,"Companion preview | read only",222,
                  &lv_font_montserrat_14,0xAAB6CA);
    pokemon_button(screen_pokemon_extras,"BACK",128,40,0,285,
                   pokemon_back_cb);

    screen_pokemon_companion=pokemon_screen();
    pokemon_label(screen_pokemon_companion,"POKEMON COMPANION",24,
                  &lv_font_montserrat_16,0xF6DFA9);
    pokemon_label(screen_pokemon_companion,"NO PLAYER LINKED  |  DEMO",52,
                  &lv_font_montserrat_14,0xDAAD7B);
    pokemon_mimikyu_preview(screen_pokemon_companion);
    demo_title=pokemon_label(screen_pokemon_companion,"MIMIKYU  #778",258,
                             &lv_font_montserrat_22,0xF8F1E2);
    demo_line1=pokemon_label(screen_pokemon_companion,"",265,
                             &lv_font_montserrat_16,0xD2DCEC);
    demo_line2=pokemon_label(screen_pokemon_companion,"",291,
                             &lv_font_montserrat_14,0x99AFC9);
    demo_hint=pokemon_label(screen_pokemon_companion,"",317,
                            &lv_font_montserrat_14,0x8794A9);
    demo_tab=pokemon_label(screen_pokemon_companion,"",234,
                           &lv_font_montserrat_14,0xC9A36F);

    lv_obj_t *prev=pokemon_button(screen_pokemon_companion,"<",41,36,
                                   -133,205,NULL);
    lv_obj_t *next=pokemon_button(screen_pokemon_companion,">",41,36,
                                   133,205,NULL);
    lv_obj_t *back=pokemon_button(screen_pokemon_companion,"BACK",72,32,
                                   -128,18,pokemon_back_cb);
    (void)back;
    lv_obj_add_event_cb(prev,pokemon_page_cb,LV_EVENT_CLICKED,(void *)(intptr_t)-1);
    lv_obj_add_event_cb(next,pokemon_page_cb,LV_EVENT_CLICKED,(void *)(intptr_t)1);
    pokemon_refresh();
}
