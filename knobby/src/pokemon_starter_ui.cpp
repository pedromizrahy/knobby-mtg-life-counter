#include "pokemon_starter_ui.h"
#include "pokemon_player_identity.h"
#include "pokemon_starter_service.h"
#include "pokemon_lab_dex.h"
#include "pokemon_lab_ball_art.h"
#include "pokemon_tpk2_renderer.h"
#include "pokemon_playgroup_preview.h"
#include "playgroup_api.h"
#include <Arduino.h>
#include <esp_random.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

/* The real starter UI is opt-in from a Playgroup player card only.
 * One saved reveal at a time; rerolls and KEEP are durable before animation.
 * If SPIFFS or SD fails, no new Pokémon is granted. */
namespace {
static lv_obj_t *screen=nullptr,*title=nullptr,*counter=nullptr;
static lv_obj_t *species=nullptr,*tier=nullptr,*status=nullptr;
static lv_obj_t *ball_img=nullptr,*sprite_img=nullptr;
static lv_obj_t *keep_btn=nullptr,*reroll_btn=nullptr;
static lv_timer_t *ball_timer=nullptr;
static uint32_t user_id=0;
static pk_starter_state_t state={};
static pk_gacha_entry_t installed[PK_LAB_DEX_COUNT]={};
static size_t installed_count=0;
static uint8_t ball_pixels[PK_LAB_BALL_BUFFER_BYTES]={};
static lv_img_dsc_t ball_desc={};
static unsigned frame=0;
static bool animating=false,ready=false,sprite_visible=false;

bool rng_secure(void *,uint32_t *out){
    if(!out)return false;
    *out=esp_random();
    return true;
}
lv_obj_t *label(const char *str,int y,unsigned size,uint32_t color){
    lv_obj_t *o=lv_label_create(screen);
    lv_label_set_text(o,str);
    lv_obj_set_width(o,280);
    lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_font(o,size==22?&lv_font_montserrat_22:&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_align(o,LV_ALIGN_TOP_MID,0,y);
    return o;
}
lv_obj_t *button(const char *str,int x,int y,int w,int h,lv_event_cb_t cb){
    lv_obj_t *o=lv_btn_create(screen);
    lv_obj_set_pos(o,x,y);
    lv_obj_set_size(o,w,h);
    lv_obj_set_style_radius(o,16,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xE6CE9B),0);
    lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *text=lv_label_create(o);
    lv_label_set_text(text,str);
    lv_obj_set_style_text_color(text,lv_color_hex(0x112735),0);
    lv_obj_set_style_text_font(text,&lv_font_montserrat_14,0);
    lv_obj_center(text);
    return o;
}
void detach_sprite(){
    if(sprite_img)lv_img_set_src(sprite_img,NULL);
    sprite_visible=false;
    pk_tpk2_idle_release();
}
void update_buttons(){
    const bool active=ready && !animating && !state.kept;
    if(keep_btn){
        if(active && sprite_visible)lv_obj_clear_flag(keep_btn,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(keep_btn,LV_OBJ_FLAG_HIDDEN);
    }
    if(reroll_btn){
        if(active && state.reveal_count<PK_STARTER_TOTAL_REVEALS)
            lv_obj_clear_flag(reroll_btn,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(reroll_btn,LV_OBJ_FLAG_HIDDEN);
    }
}
void show_current(){
    if(!ready || !pk_starter_state_valid(&state))return;
    if(ball_img)lv_obj_add_flag(ball_img,LV_OBJ_FLAG_HIDDEN);
    detach_sprite();
    const char *name=pk_lab_dex_name(state.current.species);
    const char *kind="COMMON";
    if(state.current.tier==PK_TIER_RARE)kind="RARE";
    else if(state.current.tier==PK_TIER_EPIC)kind="EPIC";
    else if(state.current.tier==PK_TIER_LEGENDARY)kind="LEGENDARY";
    char buf[80];
    snprintf(buf,sizeof(buf),"%s #%u",name?name:"POKEMON",
             (unsigned)state.current.species);
    lv_label_set_text(species,buf);
    snprintf(buf,sizeof(buf),"%s%s",kind,state.current.shiny?"  SHINY":"");
    lv_label_set_text(tier,buf);
    snprintf(buf,sizeof(buf),"POKE BALL   |   REROLLS LEFT %u/5",
             (unsigned)(PK_STARTER_TOTAL_REVEALS-state.reveal_count));
    lv_label_set_text(counter,buf);
    bool art=pk_tpk2_idle_load_species(state.current.species);
    if(art && pk_tpk2_idle_frame_count()){
        const lv_img_dsc_t *image=pk_tpk2_idle_frame(0);
        if(image){
            lv_img_set_src(sprite_img,image);
            lv_obj_set_size(sprite_img,LV_SIZE_CONTENT,LV_SIZE_CONTENT);
            unsigned width=image->header.w>image->header.h?
                           image->header.w:image->header.h;
            unsigned zoom=width?256U*140U/width:256U;
            if(zoom>512U)zoom=512U;
            lv_img_set_zoom(sprite_img,(uint16_t)zoom);
            lv_obj_align(sprite_img,LV_ALIGN_CENTER,0,-10);
            lv_obj_clear_flag(sprite_img,LV_OBJ_FLAG_HIDDEN);
            sprite_visible=true;
        }
    }
    lv_label_set_text(status,state.kept?"SAVED TO YOUR TEAM":
                     art?"KEEP OR REROLL":"SPRITE MISSING - NO KEEP");
    update_buttons();
}
void paint_ball(){
    if(!pk_lab_ball_art_render(PK_BALL_POKE,frame,
                               ball_pixels,sizeof(ball_pixels)))return;
    lv_img_cache_invalidate_src(&ball_desc);
    lv_img_set_src(ball_img,&ball_desc);
}
void ball_tick(lv_timer_t *){
    if(!animating || lv_scr_act()!=screen)return;
    if(++frame<PK_LAB_BALL_FRAMES){paint_ball();return;}
    animating=false;
    lv_timer_pause(ball_timer);
    show_current();
}
void animate_reveal(){
    detach_sprite();
    lv_label_set_text(species,"");
    lv_label_set_text(tier,"");
    lv_label_set_text(status,"OPENING POKE BALL...");
    lv_obj_clear_flag(ball_img,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sprite_img,LV_OBJ_FLAG_HIDDEN);
    frame=0;
    animating=true;
    update_buttons();
    paint_ball();
    lv_timer_set_period(ball_timer,110);
    lv_timer_reset(ball_timer);
    lv_timer_resume(ball_timer);
}
void on_back(lv_event_t *){
    if(ball_timer)lv_timer_pause(ball_timer);
    animating=false;
    detach_sprite();
    ready=false;
    user_id=0;
    pk_pg_preview_resume();
}
void on_keep(lv_event_t *){
    if(!ready || animating || state.kept || !sprite_visible)return;
    pk_starter_result_t outcome=pk_starter_service_keep(user_id,&state);
    if(outcome==PK_STARTER_ACCEPTED ||
       outcome==PK_STARTER_ALREADY_OWNED){
        lv_label_set_text(status,"POKEMON SAVED! TAP BACK");
        show_current();
    }else{
        /* KEEP may already be persisted even if creating the profile failed.
         * Never display REROLL again if the decision was committed. */
        pk_starter_state_t recovered={};
        if(pk_starter_store_inspect(user_id,&recovered)==PK_STARTER_STORE_VALID)
            state=recovered;
        update_buttons();
        lv_label_set_text(status,"SAVE FAILED - EXIT AND RETRY");
    }
}
void on_reroll(lv_event_t *){
    if(!ready || animating || state.kept ||
       state.reveal_count>=PK_STARTER_TOTAL_REVEALS)return;
    pk_starter_result_t outcome=pk_starter_service_reroll(
        user_id,installed,installed_count,rng_secure,NULL,&state);
    if(outcome==PK_STARTER_REVEALED){
        animate_reveal();
    }else{
        pk_starter_state_t recovered={};
        if(pk_starter_store_inspect(user_id,&recovered)==PK_STARTER_STORE_VALID)
            state=recovered;
        update_buttons();
        lv_label_set_text(status,"REROLL FAILED - TRY REOPENING");
    }
}
void build(){
    if(screen)return;
    screen=lv_obj_create(NULL);
    lv_obj_set_size(screen,360,360);
    lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_set_style_border_width(screen,0,0);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x0A1C2B),0);
    lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    title=label("YOUR STARTER POKEMON",26,22,0xEEDBA5);
    counter=label("POKE BALL | REROLLS LEFT 5/5",55,14,0xA9C4C4);
    button("BACK",137,84,86,28,on_back);
    ball_img=lv_img_create(screen);
    lv_obj_align(ball_img,LV_ALIGN_CENTER,0,-10);
    lv_obj_add_flag(ball_img,LV_OBJ_FLAG_HIDDEN);
    sprite_img=lv_img_create(screen);
    lv_obj_add_flag(sprite_img,LV_OBJ_FLAG_HIDDEN);
    species=label("",233,22,0xF4E5C2);
    tier=label("",261,14,0xD5A96B);
    status=label("VERIFYING PLAYGROUP ACCOUNT...",280,14,0x96BCB4);
    keep_btn=button("KEEP",90,295,82,34,on_keep);
    reroll_btn=button("REROLL",188,295,82,34,on_reroll);
    /* Both button corners remain inside round glass at y=329. */
    lv_obj_add_flag(keep_btn,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(reroll_btn,LV_OBJ_FLAG_HIDDEN);
    ball_desc.header.always_zero=0;
    ball_desc.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
    ball_desc.header.w=PK_LAB_BALL_PIXELS;
    ball_desc.header.h=PK_LAB_BALL_PIXELS;
    ball_desc.data_size=sizeof(ball_pixels);
    ball_desc.data=ball_pixels;
    ball_timer=lv_timer_create(ball_tick,110,NULL);
    lv_timer_pause(ball_timer);
}
bool prepare_catalog(){
    uint16_t ids[252]={};
    size_t n=pk_tpk2_list_installed_gen12(ids,252);
    installed_count=0;
    for(size_t i=0;i<n;i++){
        uint8_t rarity=pk_lab_dex_tier(ids[i]);
        if(rarity>=PK_TIER_COUNT)continue;
        if(installed_count>=PK_LAB_DEX_COUNT)return false;
        installed[installed_count++]={ids[i],rarity,1U};
    }
    return pk_gacha_catalog_ready(PK_BALL_POKE,installed,installed_count);
}
} // namespace

extern "C" bool pk_starter_ui_open(uint32_t expected_id)
{
    if(!expected_id || !pk_player_id_valid(expected_id))return false;
    build();
    if(!screen || lv_scr_act()==screen)return false;
    ready=false;animating=false;user_id=0;
    detach_sprite();
    if(ball_timer)lv_timer_pause(ball_timer);
    lv_label_set_text(counter,"POKE BALL  |  5 REROLLS");
    lv_label_set_text(species,"");
    lv_label_set_text(tier,"");
    lv_label_set_text(status,"VERIFYING PLAYGROUP ACCOUNT...");
    lv_obj_add_flag(ball_img,LV_OBJ_FLAG_HIDDEN);
    update_buttons();
    lv_scr_load(screen);
    lv_refr_now(NULL);
    uint32_t verified=0;
    if(!playgroup_authenticated_user_id(&verified) ||
       verified!=expected_id){
        lv_label_set_text(status,"NOT YOUR PLAYGROUP ACCOUNT");
        return true;
    }
    user_id=verified;
    pk_starter_state_t restored={};
    pk_starter_store_status_t st=pk_starter_store_inspect(user_id,&restored);
    if(st==PK_STARTER_STORE_ERROR || st==PK_STARTER_STORE_CORRUPT){
        lv_label_set_text(status,"SAVED ROLLS ERROR - NO CHANGES");
        return true;
    }
    if(st==PK_STARTER_STORE_MISSING && !prepare_catalog()){
        lv_label_set_text(status,"POKEMON SD CATALOG NOT READY");
        return true;
    }
    if(st==PK_STARTER_STORE_VALID && !restored.kept && !prepare_catalog()){
        lv_label_set_text(status,"POKEMON SD CATALOG NOT READY");
        return true;
    }
    pk_starter_result_t result=pk_starter_service_open(
        user_id,installed,installed_count,rng_secure,NULL,&state);
    if(result!=PK_STARTER_REVEALED &&
       result!=PK_STARTER_ACCEPTED &&
       result!=PK_STARTER_ALREADY_OWNED){
        lv_label_set_text(status,"ACCOUNT ALREADY LINKED OR BLOCKED");
        return true;
    }
    ready=true;
    if(result==PK_STARTER_REVEALED && st==PK_STARTER_STORE_MISSING)
        animate_reveal();
    else show_current();
    return true;
}
