#include "pokemon_lab_ui.h"
#include "pokemon_lab_runtime.h"
#include "pokemon_lab_ball_art.h"
#include "pokemon_lab_reveal.h"
#include "pokemon_lab_fixture_art.h"
#include "pokemon_tpk2_renderer.h"
#include "pokemon_ui.h"
#include <lvgl.h>
#include <stdio.h>
#include <stdint.h>

/* A dedicated single-purpose LAB: choose a ball, then watch a fullscreen
 * opening and fullscreen reveal. No inventory, profile or Playgroup writes.
 * Imported TPK2 frames are a read-only preview of installed artwork. */
static lv_obj_t *lab_screen=NULL,*opening=NULL;
static lv_obj_t *chosen_label=NULL,*count_label=NULL,*status_label=NULL;
static lv_obj_t *ball_image=NULL,*pokemon_image=NULL,*close_button=NULL;
static lv_obj_t *name_label=NULL,*tier_label=NULL,*missing_label=NULL;
static lv_timer_t *ball_timer=NULL,*sprite_timer=NULL,*session_timer=NULL;
static uint8_t ball_pixels[PK_LAB_BALL_BUFFER_BYTES];
static lv_img_dsc_t ball_dsc={0};
static uint8_t symbol_pixels[PK_LAB_FIXTURE_BUFFER_BYTES];
static lv_img_dsc_t symbol_dsc={0};
static pk_lab_t captured={0};
static uint32_t draw_session=0,shown_session=0;
static uint16_t sprite_species=0;
static unsigned frame=0,sprite_frame=0;
static uint8_t ball=PK_BALL_POKE;
static unsigned count=1;
static bool opening_active=false,reveal_active=false,sprite_attached=false;
static const char *const names[]={"POKE BALL","GREAT BALL","ULTRA BALL","MASTER BALL"};

static lv_obj_t *label(lv_obj_t *parent,const char *value,int y,int w,
                       const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent);
    lv_label_set_text(o,value);
    lv_obj_set_width(o,w);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_align(o,LV_ALIGN_TOP_MID,0,y);
    return o;
}
static lv_obj_t *button(lv_obj_t *parent,const char *title,int x,int y,
                        int w,int h,lv_event_cb_t cb,void *data)
{
    lv_obj_t *o=lv_btn_create(parent);
    lv_obj_set_pos(o,x,y);
    lv_obj_set_size(o,w,h);
    lv_obj_set_style_radius(o,16,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xECDAB3),0);
    lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,data);
    lv_obj_t *txt=lv_label_create(o);
    lv_label_set_text(txt,title);
    lv_obj_set_style_text_font(txt,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(txt,lv_color_hex(0x152735),0);
    lv_obj_center(txt);
    return o;
}
static void stop_sprite(void)
{
    if(sprite_timer)lv_timer_pause(sprite_timer);
    if(sprite_attached && pokemon_image)lv_img_set_src(pokemon_image,NULL);
    sprite_attached=false;
    sprite_species=0;
    sprite_frame=0;
}
static void update_menu(void)
{
    if(chosen_label)lv_label_set_text(chosen_label,names[ball]);
    if(count_label)lv_label_set_text(count_label,count==1?"QUANTITY x1":"QUANTITY x100");
}
static void stop_opening(void)
{
    opening_active=false;
    reveal_active=false;
    if(ball_timer)lv_timer_pause(ball_timer);
    stop_sprite();
    if(opening)lv_obj_add_flag(opening,LV_OBJ_FLAG_HIDDEN);
    if(pokemon_image)lv_obj_add_flag(pokemon_image,LV_OBJ_FLAG_HIDDEN);
    if(ball_image)lv_obj_add_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
}
static void paint_ball(unsigned step)
{
    if(!pk_lab_ball_art_render(ball,step,ball_pixels,sizeof(ball_pixels)))return;
    lv_img_cache_invalidate_src(&ball_dsc);
    lv_img_set_src(ball_image,&ball_dsc);
}
static void sprite_tick(lv_timer_t *timer)
{
    (void)timer;
    if(!reveal_active || !sprite_attached ||
       lv_scr_act()!=lab_screen ||
       pk_tpk2_idle_loaded_species()!=sprite_species ||
       pk_tpk2_idle_frame_count()==0){
        if(sprite_timer)lv_timer_pause(sprite_timer);
        return;
    }
    sprite_frame=(sprite_frame+1U)%pk_tpk2_idle_frame_count();
    const lv_img_dsc_t *img=pk_tpk2_idle_frame(sprite_frame);
    if(!img){stop_sprite();return;}
    lv_img_set_src(pokemon_image,img);
    lv_timer_set_period(sprite_timer,pk_tpk2_idle_duration_ms(sprite_frame));
}
static void reveal(void)
{
    if(!captured.has_last)return;
    const pk_gacha_result_t *r=&captured.last;
    const char *species_name=pk_lab_fixture_name(r->species);
    const char *tier=pk_lab_tier_name(r->tier);
    if(!species_name || !tier)return;
    char text[64];
    snprintf(text,sizeof(text),"%s",species_name);
    lv_label_set_text(name_label,text);
    snprintf(text,sizeof(text),"#%04u  |  %s%s",(unsigned)r->species,
             tier,r->shiny?"  SHINY":"");
    lv_label_set_text(tier_label,text);
    lv_obj_add_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pokemon_image,LV_OBJ_FLAG_HIDDEN);
    stop_sprite();

    /* Preview only genuine, normal assets installed on the SD. A Shiny
     * without a separately validated sprite must never masquerade as one. */
    bool real=!r->shiny && pokemon_ui_show_species(r->species);
    if(real && pk_tpk2_idle_frame_count()){
        const lv_img_dsc_t *img=pk_tpk2_idle_frame(0);
        if(img){
            lv_img_set_src(pokemon_image,img);
            unsigned m=img->header.w>img->header.h?
                       img->header.w:img->header.h;
            unsigned zoom=m?256U*180U/m:256U;
            if(zoom>512U)zoom=512U;
            if(zoom<1U)zoom=1U;
            lv_img_set_zoom(pokemon_image,(uint16_t)zoom);
            lv_obj_set_size(pokemon_image,184,184);
            lv_obj_align(pokemon_image,LV_ALIGN_TOP_MID,0,70);
            sprite_attached=true;
            sprite_species=r->species;
            sprite_frame=0;
            if(sprite_timer && pk_tpk2_idle_frame_count()>1){
                lv_timer_set_period(sprite_timer,pk_tpk2_idle_duration_ms(0));
                lv_timer_reset(sprite_timer);
                lv_timer_resume(sprite_timer);
            }
        }else real=false;
    }
    if(!real){
        /* Missing asset is clearly disclosed. No fake Pokemon sprite. */
        (void)pk_lab_fixture_art(r->species,r->shiny!=0,
                                 symbol_pixels,sizeof(symbol_pixels));
        lv_img_cache_invalidate_src(&symbol_dsc);
        lv_img_set_src(pokemon_image,&symbol_dsc);
        lv_img_set_zoom(pokemon_image,512);
        lv_obj_set_size(pokemon_image,96,96);
        lv_obj_align(pokemon_image,LV_ALIGN_TOP_MID,0,102);
    }
    lv_label_set_text(missing_label,real?"SD SPRITE | TEST ONLY":
                             "SPRITE NOT INSTALLED | LAB ART");
    reveal_active=true;
    if(close_button)lv_obj_clear_flag(close_button,LV_OBJ_FLAG_HIDDEN);
}
static void ball_tick(lv_timer_t *timer)
{
    (void)timer;
    if(!opening_active)return;
    uint32_t now=0;
    if(lv_scr_act()!=lab_screen ||
       !pk_lab_runtime_session_token(&now) || now!=draw_session){
        stop_opening();
        return;
    }
    frame++;
    if(frame<PK_LAB_BALL_FRAMES){
        paint_ball(frame);
        return;
    }
    opening_active=false;
    lv_timer_pause(ball_timer);
    reveal();
}
static void on_close(lv_event_t *event)
{
    (void)event;
    stop_opening();
    update_menu();
}
static void on_ball(lv_event_t *event)
{
    if(opening_active || reveal_active)return;
    unsigned i=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(i>=PK_BALL_COUNT)return;
    ball=(uint8_t)i;
    update_menu();
}
static void on_quantity(lv_event_t *event)
{
    (void)event;
    if(opening_active || reveal_active)return;
    count=count==1?100:1;
    update_menu();
}
static void on_draw(lv_event_t *event)
{
    (void)event;
    if(opening_active || reveal_active)return;
    if(!pk_lab_runtime_draw_snapshot_token(ball,count,&captured,&draw_session)){
        lv_label_set_text(status_label,"DRAW FAILED - CHECK LAB");
        return;
    }
    lv_label_set_text(status_label,"SIMULATED - NO ITEMS SPENT");
    lv_obj_clear_flag(opening,LV_OBJ_FLAG_HIDDEN);
    if(close_button)lv_obj_add_flag(close_button,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pokemon_image,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(name_label,"");
    lv_label_set_text(tier_label,"");
    lv_label_set_text(missing_label,"");
    frame=0;
    opening_active=true;
    reveal_active=false;
    stop_sprite();
    paint_ball(0);
    lv_timer_set_period(ball_timer,220);
    lv_timer_reset(ball_timer);
    lv_timer_resume(ball_timer);
}
static void on_back(lv_event_t *event)
{
    (void)event;
    stop_opening();
    if(session_timer)lv_timer_pause(session_timer);
    /* Restore the chosen RAM-only partner preview after LAB cache swaps. */
    pokemon_ui_restore_preview();
    if(screen_pokemon_companion)lv_scr_load(screen_pokemon_companion);
}
static void session_tick(lv_timer_t *timer)
{
    (void)timer;
    if(lv_scr_act()!=lab_screen)return;
    uint32_t now=0;
    if(!pk_lab_runtime_session_token(&now))return;
    if(now!=shown_session){
        shown_session=now;
        stop_opening();
        lv_label_set_text(status_label,"LAB SESSION RESET VIA USB");
    }
}
void pokemon_lab_ui_open(void)
{
    if(!lab_screen || lv_scr_act()!=screen_pokemon_companion)return;
    pk_lab_t snapshot={0};
    if(!pk_lab_runtime_snapshot(&snapshot))return;
    if(!snapshot.active && !pk_lab_runtime_start())return;
    if(!pk_lab_runtime_session_token(&shown_session))return;
    stop_opening();
    ball=PK_BALL_POKE;
    count=1;
    update_menu();
    lv_label_set_text(status_label,"RAM LAB - UNLIMITED TEST BALLS");
    lv_scr_load(lab_screen);
    if(session_timer)lv_timer_resume(session_timer);
}
void pokemon_lab_ui_build(void)
{
    if(lab_screen)return;
    lab_screen=lv_obj_create(NULL);
    lv_obj_set_size(lab_screen,360,360);
    lv_obj_set_style_pad_all(lab_screen,0,0);
    lv_obj_set_style_border_width(lab_screen,0,0);
    lv_obj_set_style_bg_color(lab_screen,lv_color_hex(0x111E31),0);
    lv_obj_clear_flag(lab_screen,LV_OBJ_FLAG_SCROLLABLE);

    label(lab_screen,"POKEBALL LAB",24,250,&lv_font_montserrat_22,0xFFFFFF);
    label(lab_screen,"CHOOSE A BALL",66,235,&lv_font_montserrat_14,0xB5C4DE);
    const char *const short_names[]={"POKE","GREAT","ULTRA","MASTER"};
    for(unsigned i=0;i<PK_BALL_COUNT;i++)
        button(lab_screen,short_names[i],27+(int)i*77,104,74,37,
               on_ball,(void *)(uintptr_t)i);
    chosen_label=label(lab_screen,"POKE BALL",159,280,
                       &lv_font_montserrat_22,0xF6DBAA);
    button(lab_screen,"OPEN BALL",92,203,176,49,on_draw,NULL);
    button(lab_screen,"QTY",119,267,122,32,on_quantity,NULL);
    count_label=label(lab_screen,"QUANTITY x1",304,250,
                      &lv_font_montserrat_14,0xD3DDEE);
    button(lab_screen,"BACK",26,263,80,34,on_back,NULL);
    status_label=label(lab_screen,"UNLIMITED | TEST ONLY",334,290,
                       &lv_font_montserrat_14,0xB5C4DE);

    /* This fullscreen scene has NO other menu controls on it. */
    opening=lv_obj_create(lab_screen);
    lv_obj_set_size(opening,360,360);
    lv_obj_set_pos(opening,0,0);
    lv_obj_set_style_pad_all(opening,0,0);
    lv_obj_set_style_border_width(opening,0,0);
    lv_obj_set_style_bg_color(opening,lv_color_hex(0x101728),0);
    lv_obj_clear_flag(opening,LV_OBJ_FLAG_SCROLLABLE);
    ball_dsc.header.w=PK_LAB_BALL_PIXELS;
    ball_dsc.header.h=PK_LAB_BALL_PIXELS;
    ball_dsc.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
    ball_dsc.data_size=sizeof(ball_pixels);
    ball_dsc.data=ball_pixels;
    ball_image=lv_img_create(opening);
    lv_img_set_src(ball_image,&ball_dsc);
    lv_img_set_zoom(ball_image,768);
    lv_obj_align(ball_image,LV_ALIGN_CENTER,0,-10);

    symbol_dsc.header.w=PK_LAB_FIXTURE_PIXELS;
    symbol_dsc.header.h=PK_LAB_FIXTURE_PIXELS;
    symbol_dsc.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
    symbol_dsc.data_size=sizeof(symbol_pixels);
    symbol_dsc.data=symbol_pixels;
    pokemon_image=lv_img_create(opening);
    lv_img_set_src(pokemon_image,&symbol_dsc);
    lv_obj_add_flag(pokemon_image,LV_OBJ_FLAG_HIDDEN);
    name_label=label(opening,"",250,315,&lv_font_montserrat_22,0xFFFFFF);
    tier_label=label(opening,"",284,315,&lv_font_montserrat_14,0xF4DEB0);
    missing_label=label(opening,"",310,315,&lv_font_montserrat_14,0xA9C0DC);
    close_button=button(opening,"CLOSE",129,321,102,32,on_close,NULL);
    lv_obj_add_flag(opening,LV_OBJ_FLAG_HIDDEN);
    ball_timer=lv_timer_create(ball_tick,220,NULL);
    lv_timer_pause(ball_timer);
    sprite_timer=lv_timer_create(sprite_tick,160,NULL);
    lv_timer_pause(sprite_timer);
    session_timer=lv_timer_create(session_tick,250,NULL);
    lv_timer_pause(session_timer);
}
