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

/* All buttons operate on the isolated, synthetic shared RAM lab.
 * No asset writes, SD access, Playgroup IDs, or actual Pokeball debit.
 * The plain text reveal intentionally does not impersonate installed art.
 */
static lv_obj_t *lab_screen=NULL;
static lv_obj_t *badge_label=NULL;
static lv_obj_t *ball_label=NULL;
static lv_obj_t *reveal_label=NULL;
static lv_obj_t *rates_label=NULL;
static lv_obj_t *count_label=NULL;
static lv_obj_t *reveal_card=NULL;
static lv_obj_t *reveal_title=NULL;
static lv_obj_t *reveal_detail=NULL;
static lv_obj_t *fixture_icon=NULL;
static uint8_t fixture_pixels[PK_LAB_FIXTURE_BUFFER_BYTES];
static lv_img_dsc_t fixture_descriptor={0};
static bool lab_sprite_attached=false;
static lv_timer_t *sprite_timer=NULL;
static unsigned sprite_frame=0;
static uint16_t sprite_species=0;
static pk_lab_t captured_draw={0};
static bool captured_valid=false;
static uint32_t captured_session=0;
static lv_obj_t *ball_image=NULL;
static lv_timer_t *ball_timer=NULL;
static unsigned animation_frame=0;
static bool animating=false;
static uint8_t ball_pixels[PK_LAB_BALL_BUFFER_BYTES];
static lv_img_dsc_t ball_descriptor={0};
static uint8_t active_ball=PK_BALL_POKE;
static unsigned batch=1;
static const char *const ball_names[]={"POKE","GREAT","ULTRA","MASTER"};
static const char *const tier_names[]={"COMMON","RARE","EPIC","LEGENDARY"};

static lv_obj_t *lab_text(lv_obj_t *parent,const char *text,
                           int y,int width,uint32_t color,const lv_font_t *font)
{
    lv_obj_t *label=lv_label_create(parent);
    lv_obj_set_width(label,width);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    lv_label_set_text(label,text);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(label,lv_color_hex(color),0);
    lv_obj_set_style_text_font(label,font,0);
    lv_obj_align(label,LV_ALIGN_TOP_MID,0,y);
    return label;
}
static lv_obj_t *lab_panel(int x,int y,int w,int h)
{
    lv_obj_t *panel=lv_obj_create(lab_screen);
    lv_obj_set_pos(panel,x,y);
    lv_obj_set_size(panel,w,h);
    lv_obj_set_style_radius(panel,14,0);
    lv_obj_set_style_bg_color(panel,lv_color_hex(0x19303A),0);
    lv_obj_set_style_border_color(panel,lv_color_hex(0x446070),0);
    lv_obj_set_style_border_width(panel,1,0);
    lv_obj_set_style_pad_all(panel,0,0);
    lv_obj_set_scrollbar_mode(panel,LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}
static lv_obj_t *lab_btn(lv_obj_t *parent,const char *title,
                           int x,int y,int w,int h,lv_event_cb_t callback,
                           void *user_data)
{
    lv_obj_t *btn=lv_btn_create(parent);
    lv_obj_set_pos(btn,x,y);
    lv_obj_set_size(btn,w,h);
    lv_obj_set_style_radius(btn,12,0);
    lv_obj_set_style_bg_color(btn,lv_color_hex(0xEAD6A4),0);
    lv_obj_set_style_shadow_width(btn,0,0);
    lv_obj_add_event_cb(btn,callback,LV_EVENT_CLICKED,user_data);
    lv_obj_t *label=lv_label_create(btn);
    lv_label_set_text(label,title);
    lv_obj_set_style_text_font(label,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(label,lv_color_hex(0x183039),0);
    lv_obj_center(label);
    return btn;
}
static void lab_refresh(void)
{
    if(!lab_screen)return;
    pk_lab_t state={0};
    if(!pk_lab_runtime_snapshot(&state) || !state.active){
        if(badge_label)lv_label_set_text(badge_label,"RAM LAB NOT STARTED");
        if(reveal_label)lv_label_set_text(reveal_label,"START LAB TO TEST");
        if(rates_label)lv_label_set_text(rates_label,"NO DATA");
        return;
    }
    char msg[160];
    if(badge_label){
        snprintf(msg,sizeof(msg),"LV %u  |  MASTER BADGE: %s",
                 (unsigned)state.simulated_level,
                 state.master_trainer_badge?"YES":"NO");
        lv_label_set_text(badge_label,msg);
    }
    if(ball_label)lv_label_set_text(ball_label,ball_names[active_ball]);
    if(count_label)lv_label_set_text(count_label,batch==1?"x100":"x1");
    if(reveal_label){
        if(state.has_last){
            snprintf(msg,sizeof(msg),"LAST #%u  %s%s (SIM)",
                     (unsigned)state.last.species,
                     tier_names[state.last.tier],
                     state.last.shiny?" SHINY":"");
        }else snprintf(msg,sizeof(msg),"NO DRAWS YET - SIMULATION");
        lv_label_set_text(reveal_label,msg);
    }
    if(rates_label){
        const uint64_t total=state.draws[active_ball];
        const uint64_t common=state.tiers[active_ball][PK_TIER_COMMON];
        const uint64_t rare=state.tiers[active_ball][PK_TIER_RARE];
        const uint64_t epic=state.tiers[active_ball][PK_TIER_EPIC];
        const uint64_t legendary=state.tiers[active_ball][PK_TIER_LEGENDARY];
        unsigned a=total?(unsigned)(common*10000ULL/total):0U;
        unsigned b=total?(unsigned)(rare*10000ULL/total):0U;
        unsigned c=total?(unsigned)(epic*10000ULL/total):0U;
        unsigned d=total?(unsigned)(legendary*10000ULL/total):0U;
        snprintf(msg,sizeof(msg),
                 "DRAWS: %llu  SHINY: %llu\n"
                 "COMMON %u.%02u%%  RARE %u.%02u%%\n"
                 "EPIC %u.%02u%%  LEGEND %u.%02u%%",
                 (unsigned long long)total,
                 (unsigned long long)state.shinies[active_ball],
                 a/100U,a%100U,b/100U,b%100U,
                 c/100U,c%100U,d/100U,d%100U);
        lv_label_set_text(rates_label,msg);
    }
}
/* Redraw the same persistent descriptor; invalidate LVGL's decoded
 * image before mutating pixels. The animation never consumes currency. */
static void ball_paint(unsigned frame)
{
    if(!ball_image || !pk_lab_ball_art_render(active_ball,frame,
                                              ball_pixels,sizeof(ball_pixels)))
        return;
    lv_img_cache_invalidate_src(&ball_descriptor);
    lv_img_set_src(ball_image,&ball_descriptor);
}
/* Detach LAB's references before the companion renderer frees its
 * shared PSRAM frame cache. No Playgroup data and no SD writes. */
static void lab_detach_sprite(void)
{
    /* Never let an LVGL timer retain or redraw a freed TPK2 descriptor. */
    if(sprite_timer)lv_timer_pause(sprite_timer);
    if(fixture_icon && lab_sprite_attached)
        lv_img_set_src(fixture_icon,NULL);
    lab_sprite_attached=false;
    sprite_species=0;
    sprite_frame=0;
}
static void lab_sprite_tick(lv_timer_t *timer)
{
    (void)timer;
    if(!lab_sprite_attached || lv_scr_act()!=lab_screen ||
       !reveal_card || lv_obj_has_flag(reveal_card,LV_OBJ_FLAG_HIDDEN)){
        if(sprite_timer)lv_timer_pause(sprite_timer);
        return;
    }
    if(pk_tpk2_idle_loaded_species()!=sprite_species ||
       pk_tpk2_idle_frame_count()==0){
        /* Fail closed: an external renderer change invalidated our cache. */
        lab_detach_sprite();
        if(fixture_icon)lv_obj_add_flag(fixture_icon,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    sprite_frame=(sprite_frame+1U)%pk_tpk2_idle_frame_count();
    const lv_img_dsc_t *frame=pk_tpk2_idle_frame(sprite_frame);
    if(!frame){lab_detach_sprite();return;}
    lv_img_set_src(fixture_icon,frame);
    lv_timer_set_period(sprite_timer,
                        pk_tpk2_idle_duration_ms(sprite_frame));
}
static void lab_restore_companion(void)
{
    lab_detach_sprite();
    /* The companion page must not inherit a laboratory-only selection. */
    (void)pokemon_ui_show_species(778U);
}
static void show_reveal_card(void)
{
    if(!reveal_card || !captured_valid || !captured_draw.has_last)return;
    uint32_t session=0;
    if(!pk_lab_runtime_session_token(&session) ||
       session!=captured_session)return;
    const pk_gacha_result_t *r=&captured_draw.last;
    char details[80];
    if(!pk_lab_reveal_text(r,details,sizeof(details)))return;
    const char *name=pk_lab_fixture_name(r->species);
    static const uint32_t accent[]={
        0xA5D7B9,0x7CBCE7,0xB9A2F3,0xE9C26D
    };
    if(!name || r->tier>=PK_TIER_COUNT)return;
    char label[48];
    snprintf(label,sizeof(label),"%s  #%u",name,(unsigned)r->species);
    lv_label_set_text(reveal_title,label);
    snprintf(label,sizeof(label),"%s%s | SIM ONLY",
             pk_lab_tier_name(r->tier),r->shiny?" *SHINY*":"");
    /* Preview the user's installed Idle sprite when available. Failed
     * read-only SD lookups fall back to our original LAB emblem. The
     * companion sprite cache is shared, so release LAB references FIRST. */
    lab_detach_sprite();
    /* A normal TPK2 does not prove a shiny variant exists. Never
     * mislabel ordinary art as shiny; use the shiny LAB emblem instead. */
    bool real_sprite=!r->shiny && pokemon_ui_show_species(r->species);
    if(fixture_icon && real_sprite && pk_tpk2_idle_frame_count()>0){
        const lv_img_dsc_t *frame=pk_tpk2_idle_frame(0);
        if(frame){
            lv_img_set_src(fixture_icon,frame);
            const unsigned max_dimension=
                frame->header.w>frame->header.h?
                frame->header.w:frame->header.h;
            unsigned zoom=max_dimension?
                (256U*38U)/max_dimension:256U;
            if(zoom<1U)zoom=1U;
            if(zoom>256U)zoom=256U;
            lv_img_set_zoom(fixture_icon,(uint16_t)zoom);
            lv_obj_set_size(fixture_icon,40,40);
            lv_obj_align(fixture_icon,LV_ALIGN_LEFT_MID,7,0);
            lab_sprite_attached=true;
            sprite_species=r->species;
            sprite_frame=0;
            lv_obj_clear_flag(fixture_icon,LV_OBJ_FLAG_HIDDEN);
            if(sprite_timer && pk_tpk2_idle_frame_count()>1U){
                lv_timer_set_period(sprite_timer,pk_tpk2_idle_duration_ms(0));
                lv_timer_reset(sprite_timer);
                lv_timer_resume(sprite_timer);
            }
        } else real_sprite=false;
    }
    if(!real_sprite && fixture_icon &&
       pk_lab_fixture_art(r->species,r->shiny!=0,
                          fixture_pixels,sizeof(fixture_pixels))){
        lv_img_cache_invalidate_src(&fixture_descriptor);
        lv_img_set_zoom(fixture_icon,256);
        lv_img_set_src(fixture_icon,&fixture_descriptor);
        lv_obj_set_size(fixture_icon,40,40);
        lv_obj_align(fixture_icon,LV_ALIGN_LEFT_MID,7,0);
        lv_obj_clear_flag(fixture_icon,LV_OBJ_FLAG_HIDDEN);
    }
    snprintf(label,sizeof(label),"%s%s | %s",
             pk_lab_tier_name(r->tier),r->shiny?" SHINY":"",
             real_sprite?"SD PREVIEW":"LAB ART");
    lv_label_set_text(reveal_detail,label);
    lv_obj_set_style_border_color(reveal_card,lv_color_hex(accent[r->tier]),0);
    lv_obj_set_style_text_color(reveal_title,
                    lv_color_hex(r->shiny?0xFDEBA4:accent[r->tier]),0);
    lv_obj_clear_flag(reveal_card,LV_OBJ_FLAG_HIDDEN);
}
static void ball_tick(lv_timer_t *timer)
{
    (void)timer;
    if(!animating)return;
    uint32_t current_session=0;
    if(!pk_lab_runtime_session_token(&current_session) ||
       !captured_valid || current_session!=captured_session){
        /* USB START/RESET invalidates the pending reveal. Do not present
         * an old result in a newly reset LAB session. */
        animating=false;
        if(ball_timer)lv_timer_pause(ball_timer);
        if(ball_image)lv_obj_add_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
        if(reveal_card)lv_obj_add_flag(reveal_card,LV_OBJ_FLAG_HIDDEN);
        if(reveal_label)lv_obj_clear_flag(reveal_label,LV_OBJ_FLAG_HIDDEN);
        if(rates_label)lv_obj_clear_flag(rates_label,LV_OBJ_FLAG_HIDDEN);
        captured_valid=false;
        lab_refresh();
        return;
    }
    if(lv_scr_act()!=lab_screen){
        animating=false;
        lv_timer_pause(ball_timer);
        lv_obj_add_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    animation_frame++;
    if(animation_frame>=PK_LAB_BALL_FRAMES){
        animating=false;
        lv_timer_pause(ball_timer);
        lv_obj_add_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(reveal_label,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(rates_label,LV_OBJ_FLAG_HIDDEN);
        /* The card shows this DRAW's frozen result, not a concurrent
         * serial draw. Statistics may reflect newer serial activity. */
        lab_refresh();
        show_reveal_card();
        return;
    }
    ball_paint(animation_frame);
}
static void ball_begin(void)
{
    if(!ball_timer || !ball_image)return;
    animation_frame=0;
    animating=true;
    if(reveal_card)lv_obj_add_flag(reveal_card,LV_OBJ_FLAG_HIDDEN);
    lab_detach_sprite();
    lv_obj_add_flag(reveal_label,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rates_label,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
    ball_paint(0);
    lv_timer_set_period(ball_timer,140);
    lv_timer_reset(ball_timer);
    lv_timer_resume(ball_timer);
}
static void on_ball(lv_event_t *event)
{
    unsigned chosen=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(animating || chosen>=PK_BALL_COUNT)return;
    active_ball=(uint8_t)chosen;
    if(reveal_card)lv_obj_add_flag(reveal_card,LV_OBJ_FLAG_HIDDEN);
    lab_detach_sprite();
    lab_refresh();
}
static void on_draw(lv_event_t *event)
{
    (void)event;
    if(animating)return;
    if(!pk_lab_runtime_draw_snapshot_token(active_ball,batch,
                                            &captured_draw,&captured_session)) {
        if(reveal_label)lv_label_set_text(reveal_label,"DRAW FAILED (SIM)");
    } else {
        captured_valid=true;
        ball_begin();
    }
}
static void on_batch(lv_event_t *event)
{
    (void)event;
    if(animating)return;
    batch=batch==1?100:1;
    lab_refresh();
}
static void on_badge(lv_event_t *event)
{
    (void)event;
    if(!animating && pk_lab_runtime_level(100))lab_refresh();
}
static void on_reset(lv_event_t *event)
{
    (void)event;
    if(animating || !pk_lab_runtime_start())return;
    /* A reset must never leave an old result or sprite on screen. */
    captured_valid=false;
    lab_detach_sprite();
    if(reveal_card)lv_obj_add_flag(reveal_card,LV_OBJ_FLAG_HIDDEN);
    lab_refresh();
}
static void on_back(lv_event_t *event)
{
    (void)event;
    if(ball_timer)lv_timer_pause(ball_timer);
    animating=false;
    if(reveal_card)lv_obj_add_flag(reveal_card,LV_OBJ_FLAG_HIDDEN);
    lab_restore_companion();
    if(ball_image)lv_obj_add_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
    if(reveal_label)lv_obj_clear_flag(reveal_label,LV_OBJ_FLAG_HIDDEN);
    if(rates_label)lv_obj_clear_flag(rates_label,LV_OBJ_FLAG_HIDDEN);
    /* Returning to existing companion does not restart or clear LAB. */
    if(screen_pokemon_companion)lv_scr_load(screen_pokemon_companion);
}
void pokemon_lab_ui_open(void)
{
    if(!lab_screen || !screen_pokemon_companion ||
       lv_scr_act()!=screen_pokemon_companion)return;
    pk_lab_t state={0};
    if(!pk_lab_runtime_snapshot(&state))return;
    if(!state.active && !pk_lab_runtime_start())return;
    active_ball=PK_BALL_POKE;
    batch=1;
    captured_valid=false;
    lab_detach_sprite();
    if(reveal_card)lv_obj_add_flag(reveal_card,LV_OBJ_FLAG_HIDDEN);
    animating=false;
    if(ball_timer)lv_timer_pause(ball_timer);
    if(ball_image)lv_obj_add_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
    if(reveal_label)lv_obj_clear_flag(reveal_label,LV_OBJ_FLAG_HIDDEN);
    if(rates_label)lv_obj_clear_flag(rates_label,LV_OBJ_FLAG_HIDDEN);
    lab_refresh();
    lv_scr_load(lab_screen);
}
void pokemon_lab_ui_build(void)
{
    if(lab_screen)return;
    lab_screen=lv_obj_create(NULL);
    lv_obj_set_size(lab_screen,360,360);
    lv_obj_set_style_pad_all(lab_screen,0,0);
    lv_obj_set_style_border_width(lab_screen,0,0);
    lv_obj_set_style_bg_color(lab_screen,lv_color_hex(0x0D1D28),0);
    lv_obj_clear_flag(lab_screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(lab_screen,LV_SCROLLBAR_MODE_OFF);

    lab_text(lab_screen,"TEST LAB",22,220,0xF4DEAD,&lv_font_montserrat_22);
    lab_text(lab_screen,"UNLIMITED BALLS - SIM ONLY",52,275,
             0x9CC8D0,&lv_font_montserrat_14);
    badge_label=lab_text(lab_screen,"LV 1 | MASTER BADGE: NO",68,275,
                         0xFFFFFF,&lv_font_montserrat_14);
    const int x[]={30,107,184,261};
    for(unsigned i=0;i<PK_BALL_COUNT;i++)
        lab_btn(lab_screen,ball_names[i],x[i],93,69,32,on_ball,
                (void *)(uintptr_t)i);

    lv_obj_t *open_panel=lab_panel(40,136,280,64);
    ball_label=lab_text(open_panel,"POKE",2,88,0xF4DEAD,
                        &lv_font_montserrat_14);
    lab_btn(open_panel,"DRAW",9,22,108,29,on_draw,NULL);
    count_label=lab_text(open_panel,"x100",2,72,0xF4DEAD,
                         &lv_font_montserrat_14);
    lab_btn(open_panel,"QTY",164,22,105,29,on_batch,NULL);
    reveal_label=lab_text(lab_screen,"NO DRAWS YET - SIMULATION",201,294,
                           0xFFFFFF,&lv_font_montserrat_14);
    lv_obj_t *stats=lab_panel(43,222,274,68);
    rates_label=lab_text(stats,"NO DATA",4,264,0xE8F1EB,
                          &lv_font_montserrat_14);
    lv_obj_set_style_text_line_space(rates_label,3,0);
    lab_btn(lab_screen,"BACK",34,298,83,33,on_back,NULL);
    lab_btn(lab_screen,"LV100",137,298,84,33,on_badge,NULL);
    lab_btn(lab_screen,"RESET",241,298,84,33,on_reset,NULL);

    /* Persistent 64x64 RGB565+A image of original 2x pixel artwork.
     * No external sprites and no allocations on animation ticks. */
    ball_descriptor.header.always_zero=0;
    ball_descriptor.header.w=PK_LAB_BALL_PIXELS;
    ball_descriptor.header.h=PK_LAB_BALL_PIXELS;
    ball_descriptor.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
    ball_descriptor.data_size=sizeof(ball_pixels);
    ball_descriptor.data=ball_pixels;
    ball_image=lv_img_create(lab_screen);
    lv_img_set_src(ball_image,&ball_descriptor);
    lv_obj_align(ball_image,LV_ALIGN_TOP_MID,0,207);
    lv_obj_add_flag(ball_image,LV_OBJ_FLAG_HIDDEN);
    ball_timer=lv_timer_create(ball_tick,140,NULL);
    lv_timer_pause(ball_timer);
    /* Non-interactive reveal layer above statistics, still strictly a
     * simulated fixture. Original 64x64 ball remains separately animated. */
    reveal_card=lv_obj_create(lab_screen);
    lv_obj_set_size(reveal_card,278,70);
    lv_obj_align(reveal_card,LV_ALIGN_TOP_MID,0,214);
    lv_obj_set_style_radius(reveal_card,15,0);
    lv_obj_set_style_bg_color(reveal_card,lv_color_hex(0x132733),0);
    lv_obj_set_style_bg_opa(reveal_card,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(reveal_card,2,0);
    lv_obj_set_style_pad_all(reveal_card,0,0);
    lv_obj_set_style_shadow_width(reveal_card,0,0);
    lv_obj_clear_flag(reveal_card,LV_OBJ_FLAG_CLICKABLE |
                                      LV_OBJ_FLAG_SCROLLABLE);
    fixture_descriptor.header.always_zero=0;
    fixture_descriptor.header.w=PK_LAB_FIXTURE_PIXELS;
    fixture_descriptor.header.h=PK_LAB_FIXTURE_PIXELS;
    fixture_descriptor.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
    fixture_descriptor.data_size=sizeof(fixture_pixels);
    fixture_descriptor.data=fixture_pixels;
    fixture_icon=lv_img_create(reveal_card);
    lv_img_set_src(fixture_icon,&fixture_descriptor);
    lv_obj_align(fixture_icon,LV_ALIGN_LEFT_MID,7,0);
    lv_obj_add_flag(fixture_icon,LV_OBJ_FLAG_HIDDEN);
    reveal_title=lab_text(reveal_card,"",10,210,0xFDEBA4,
                           &lv_font_montserrat_22);
    lv_obj_align(reveal_title,LV_ALIGN_TOP_LEFT,53,10);
    reveal_detail=lab_text(reveal_card,"",43,210,0xFFFFFF,
                            &lv_font_montserrat_14);
    lv_obj_align(reveal_detail,LV_ALIGN_TOP_LEFT,53,43);
    lv_obj_add_flag(reveal_card,LV_OBJ_FLAG_HIDDEN);
    sprite_timer=lv_timer_create(lab_sprite_tick,200,NULL);
    lv_timer_pause(sprite_timer);
}
