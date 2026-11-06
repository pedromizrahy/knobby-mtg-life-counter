#include "pokemon_lab_ui.h"
#include "pokemon_lab_runtime.h"
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
static void on_ball(lv_event_t *event)
{
    unsigned chosen=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    if(chosen>=PK_BALL_COUNT)return;
    active_ball=(uint8_t)chosen;
    lab_refresh();
}
static void on_draw(lv_event_t *event)
{
    (void)event;
    if(!pk_lab_runtime_draw(active_ball,batch)) {
        if(reveal_label)lv_label_set_text(reveal_label,"DRAW FAILED (SIM)");
    } else lab_refresh();
}
static void on_batch(lv_event_t *event)
{
    (void)event;
    batch=batch==1?100:1;
    lab_refresh();
}
static void on_badge(lv_event_t *event)
{
    (void)event;
    if(pk_lab_runtime_level(100))lab_refresh();
}
static void on_reset(lv_event_t *event)
{
    (void)event;
    if(pk_lab_runtime_start())lab_refresh();
}
static void on_back(lv_event_t *event)
{
    (void)event;
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
             0x9CC8D0,&lv_font_montserrat_12);
    badge_label=lab_text(lab_screen,"LV 1 | MASTER BADGE: NO",68,275,
                         0xFFFFFF,&lv_font_montserrat_12);
    const int x[]={30,107,184,261};
    for(unsigned i=0;i<PK_BALL_COUNT;i++)
        lab_btn(lab_screen,ball_names[i],x[i],93,69,32,on_ball,
                (void *)(uintptr_t)i);

    lv_obj_t *open_panel=lab_panel(40,136,280,64);
    ball_label=lab_text(open_panel,"POKE",2,88,0xF4DEAD,
                        &lv_font_montserrat_12);
    lab_btn(open_panel,"DRAW",9,22,108,29,on_draw,NULL);
    count_label=lab_text(open_panel,"x100",2,72,0xF4DEAD,
                         &lv_font_montserrat_12);
    lab_btn(open_panel,"QTY",164,22,105,29,on_batch,NULL);
    reveal_label=lab_text(lab_screen,"NO DRAWS YET - SIMULATION",205,294,
                           0xFFFFFF,&lv_font_montserrat_12);
    lv_obj_t *stats=lab_panel(43,228,274,59);
    rates_label=lab_text(stats,"NO DATA",4,264,0xE8F1EB,
                          &lv_font_montserrat_12);
    lv_obj_set_style_text_line_space(rates_label,3,0);
    lab_btn(lab_screen,"BACK",34,298,83,33,on_back,NULL);
    lab_btn(lab_screen,"LV100",137,298,84,33,on_badge,NULL);
    lab_btn(lab_screen,"RESET",241,298,84,33,on_reset,NULL);
}
