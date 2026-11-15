#include "pokemon_evolution_ui.h"
#include "pokemon_playgroup_preview.h"
#include "pokemon_player_identity.h"
#include "pokemon_file_store.h"
#include "pokemon_profile_store.h"
#include "pokemon_item_store.h"
#include "pokemon_evolution.h"
#include "pokemon_evolution_items.h"
#include "pokemon_lab_dex.h"
#include "pokemon_tpk2_renderer.h"
#include "playgroup_api.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

/* No writes, no reward claims, no evolution actions on this screen.
 * Showing requirements does not imply that a stone or Link Cable is
 * available or that a special evolution is implemented. */
namespace {
static lv_obj_t *screen=nullptr,*name_text=nullptr,*level_text=nullptr;
static lv_obj_t *image=nullptr,*message=nullptr,*page_text=nullptr;
static lv_obj_t *options[2]={},*next_btn=nullptr;
static pk_evo_rule_t routes[12]={};
static pk_item_ledger_t inventory={};
static size_t route_count=0,page=0;
static pk_mon_t mon={};
static bool sprite_attached=false,inventory_ok=false;

lv_obj_t *text_at(const char *value,int y,int width,int size,uint32_t color){
    lv_obj_t *o=lv_label_create(screen);
    lv_label_set_text(o,value);
    lv_obj_set_size(o,width,size==22?29:22);
    lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_font(o,size==22?&lv_font_montserrat_22:&lv_font_montserrat_14,0);
    lv_obj_align(o,LV_ALIGN_TOP_MID,0,y);
    return o;
}
void discard_sprite(){
    if(image)lv_img_set_src(image,NULL);
    sprite_attached=false;
    pk_tpk2_idle_release();
}
void leave(lv_event_t *){
    discard_sprite();
    pk_pg_preview_resume();
}
void render();
void next(lv_event_t *){
    if(!route_count)return;
    const size_t pages=(route_count+1U)/2U;
    page=(page+1U)%pages;
    render();
}
lv_obj_t *button(const char *value,int x,int w,lv_event_cb_t cb){
    lv_obj_t *o=lv_btn_create(screen);
    lv_obj_set_pos(o,x,308);
    lv_obj_set_size(o,w,27);
    lv_obj_set_style_radius(o,13,0);
    lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xE2D1A0),0);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *t=lv_label_create(o);
    lv_label_set_text(t,value);
    lv_obj_set_style_text_color(t,lv_color_hex(0x122635),0);
    lv_obj_set_style_text_font(t,&lv_font_montserrat_14,0);
    lv_obj_center(t);
    return o;
}
void build(){
    if(screen)return;
    screen=lv_obj_create(NULL);
    lv_obj_set_size(screen,360,360);
    lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_set_style_border_width(screen,0,0);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x0A1C2B),0);
    lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    text_at("EVOLVE - PREVIEW",27,280,22,0xEEDBA5);
    name_text=text_at("",60,280,14,0xFFFFFF);
    level_text=text_at("",82,280,14,0x8EBEBB);
    image=lv_img_create(screen);
    lv_obj_add_flag(image,LV_OBJ_FLAG_HIDDEN);
    options[0]=text_at("",222,290,14,0xFFFFFF);
    options[1]=text_at("",259,290,14,0xFFFFFF);
    message=text_at("",284,290,14,0x96B9B5);
    page_text=text_at("",190,280,14,0xEEDBA5);
    button("BACK",100,76,leave);
    next_btn=button("NEXT",184,76,next);
}
pk_item_type_t stone(uint16_t api_id){
    switch(api_id){
    case 80:return PK_ITEM_SUN_STONE;
    case 81:return PK_ITEM_MOON_STONE;
    case 82:return PK_ITEM_FIRE_STONE;
    case 83:return PK_ITEM_THUNDER_STONE;
    case 84:return PK_ITEM_WATER_STONE;
    case 85:return PK_ITEM_LEAF_STONE;
    default:return PK_ITEM_NONE;
    }
}
void requirement(const pk_evo_rule_t &rule,char out[100]){
    const char *name=pk_lab_dex_name(rule.to);
    if(!name)name="POKEMON";
    char cond[48]={};
    if(rule.kind==PK_EVO_LEVEL)
        snprintf(cond,sizeof(cond),"LV %u  %s",(unsigned)rule.level,
                 mon.level>=rule.level?"READY":"LOCKED");
    else if(rule.kind==PK_EVO_STONE){
        pk_item_type_t item=stone(rule.item_id);
        if(item!=PK_ITEM_NONE){
            snprintf(cond,sizeof(cond),"%s  x%u",pk_item_name(item),
                     inventory_ok?(unsigned)inventory.count[item]:0U);
        }else
            snprintf(cond,sizeof(cond),"SPECIAL STONE (LOCKED)");
    }else if(rule.kind==PK_EVO_TRADE){
        snprintf(cond,sizeof(cond),"LINK CABLE  x%u",
                 inventory_ok?(unsigned)inventory.count[PK_ITEM_LINK_CABLE]:0U);
    }else if(rule.kind==PK_EVO_FRIENDSHIP)
        snprintf(cond,sizeof(cond),"FRIENDSHIP (PENDING)");
    else
        snprintf(cond,sizeof(cond),"SPECIAL (PENDING)");
    snprintf(out,100,"%s #%u  |  %s",name,(unsigned)rule.to,cond);
}
void render(){
    const size_t pages=(route_count+1U)/2U;
    char buf[100];
    snprintf(buf,sizeof(buf),"LV %u  |  %s",
             (unsigned)mon.level,mon.shiny?"SHINY":"NORMAL");
    lv_label_set_text(level_text,buf);
    if(!route_count){
        lv_label_set_text(options[0],"NO EVOLUTION IN CURRENT DEX");
        lv_label_set_text(options[1],"");
        lv_label_set_text(page_text,"");
        lv_obj_add_flag(next_btn,LV_OBJ_FLAG_HIDDEN);
    }else{
        snprintf(buf,sizeof(buf),"EVOLUTIONS %u/%u",(unsigned)(page+1U),(unsigned)pages);
        lv_label_set_text(page_text,buf);
        for(size_t i=0;i<2;i++){
            size_t index=page*2U+i;
            if(index<route_count){
                requirement(routes[index],buf);
                lv_label_set_text(options[i],buf);
            }else lv_label_set_text(options[i],"");
        }
        if(pages>1U)lv_obj_clear_flag(next_btn,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(next_btn,LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(message,"READ-ONLY | NO ITEMS SPENT");
}
void draw_sprite(){
    discard_sprite();
    if(!pk_tpk2_idle_load_species(mon.species) ||
       !pk_tpk2_idle_frame_count())return;
    const lv_img_dsc_t *frame=pk_tpk2_idle_frame(0);
    if(!frame)return;
    lv_img_set_src(image,frame);
    unsigned edge=frame->header.w>frame->header.h?
                  frame->header.w:frame->header.h;
    unsigned zoom=edge?256U*74U/edge:256U;
    if(zoom>512U)zoom=512U;
    lv_img_set_zoom(image,(uint16_t)zoom);
    lv_obj_align(image,LV_ALIGN_CENTER,0,-37);
    lv_obj_clear_flag(image,LV_OBJ_FLAG_HIDDEN);
    sprite_attached=true;
}
} // namespace
extern "C" bool pk_evolution_ui_open(uint32_t expected_user_id){
    if(!pk_player_id_valid(expected_user_id))return false;
    build();
    if(lv_scr_act()==screen)return false;
    discard_sprite();
    mon={};route_count=0;page=0;
    lv_label_set_text(name_text,"");
    lv_label_set_text(level_text,"");
    lv_label_set_text(page_text,"");
    lv_scr_load(screen);
    lv_label_set_text(message,"VERIFYING PLAYGROUP /ME...");
    for(unsigned i=0;i<2;i++)lv_label_set_text(options[i],"");
    lv_obj_add_flag(next_btn,LV_OBJ_FLAG_HIDDEN);
    uint32_t authenticated=0;
    if(!playgroup_authenticated_user_id(&authenticated) ||
       authenticated!=expected_user_id){
        lv_label_set_text(message,"ACCOUNT NOT VERIFIED");
        return true;
    }
    pk_profile_t profile={};
    pk_store_status_t stored=pk_file_inspect(authenticated,&profile);
    if(stored!=PK_STORE_VALID ||
       profile.version!=1U || profile.team_count<1U ||
       profile.active_index>=profile.team_count){
        /* Older NVS is read only when canonical SPIFFS is genuinely
         * absent; a corrupt SPIFFS profile cannot be bypassed. */
        if(stored!=PK_STORE_MISSING ||
           pk_store_inspect(authenticated,&profile)!=PK_STORE_VALID ||
           profile.team_count<1U ||
           profile.active_index>=profile.team_count){
            lv_label_set_text(message,"NO VERIFIED POKEMON");
            return true;
        }
    }
    mon=profile.team[profile.active_index];
    if(mon.species<1U || mon.species>PK_DEX_SPECIES ||
       mon.level<1U || mon.level>100U || mon.shiny>1U){
        lv_label_set_text(message,"INVALID POKEMON PROFILE");
        return true;
    }
    char title[64];
    const char *name=pk_lab_dex_name(mon.species);
    snprintf(title,sizeof(title),"%s #%u",name?name:"POKEMON",
             (unsigned)mon.species);
    lv_label_set_text(name_text,title);
    inventory={};
    pk_item_store_status_t items=pk_item_store_inspect(authenticated,&inventory,NULL);
    inventory_ok=items==PK_ITEM_STORE_VALID;
    route_count=pk_evo_options(mon.species,routes,12);
    if(route_count>12U)route_count=0U;
    page=0U;
    render();
    if(items==PK_ITEM_STORE_CORRUPT || items==PK_ITEM_STORE_ERROR)
        lv_label_set_text(message,"INVENTORY READ ERROR");
    draw_sprite();
    return true;
}
