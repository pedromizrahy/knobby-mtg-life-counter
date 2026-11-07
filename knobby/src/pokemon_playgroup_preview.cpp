#include "pokemon_playgroup_preview.h"
#include "pokemon_playgroup_roster.h"
#include "pokemon_profile_store.h"
#include "pokemon_file_store.h"
#include "pokemon_lab_dex.h"
#include "../board_detect.h"
#include <Arduino.h>
#include <lvgl.h>
#include <FS.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

extern "C" lv_obj_t *screen_pregame_roster;

/* Four static TPK2 Idle frames, independent of the Companion's single-frame
 * animated cache. Never render into a shared LVGL descriptor. There is no
 * networking, player mutation, capture, reward, or game-state call here. */
namespace {
constexpr size_t kSeats=8U,kPage=4U;
constexpr unsigned kMaxSide=96U;
constexpr size_t kBytesPerPixel=sizeof(lv_color_t)+1U;
struct Card {
    lv_obj_t *root,*name,*image,*status,*species;
    lv_img_dsc_t descriptor;
    uint8_t *pixels;
};
static Card cards[kPage]={};
static pk_pg_main_t participants[kSeats]={};
static size_t participant_count=0U,page=0U;
static lv_obj_t *screen=NULL,*counter=NULL,*back_button=NULL,*next_button=NULL;
static bool mounted=false;

bool exact(File &file,void *dst,size_t count){
    return file.read((uint8_t *)dst,count)==count;
}
uint16_t le16(const uint8_t *p){
    return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}
void forget(Card &card){
    if(card.image)lv_img_set_src(card.image,NULL);
    if(card.pixels)heap_caps_free(card.pixels);
    card.pixels=NULL;
    memset(&card.descriptor,0,sizeof(card.descriptor));
}
void unload_all(){
    for(unsigned i=0;i<kPage;i++)forget(cards[i]);
}
bool sprite_path(uint16_t species,bool shiny,char out[24]){
    if(species<1U || species>PK_DEX_SPECIES)return false;
    int n=snprintf(out,24,shiny?"/mons/p%us.bin":"/mons/p%u.bin",
                   (unsigned)species);
    return n>0 && n<24;
}
/* Read the FIRST Idle frame only. All native dimensions and TPK2 bounds are
 * validated before allocating a per-card buffer in PSRAM. */
bool decode_first_idle(File &file,Card &card){
    uint32_t size=(uint32_t)file.size();
    uint8_t head[7]={};
    if(size<7U || size>3U*1024U*1024U ||
       !exact(file,head,sizeof(head)) || memcmp(head,"TPK2",4)!=0)
        return false;
    unsigned acts=head[4],palette_count=le16(head+5);
    if(acts<1U || acts>12U || palette_count<1U || palette_count>255U ||
       7U+palette_count*2U>size)return false;
    lv_color_t colors[256]={};
    for(unsigned i=0;i<palette_count;i++){
        uint8_t raw[2];
        if(!exact(file,raw,2U))return false;
        uint16_t col=le16(raw);
        uint8_t r=(uint8_t)((((col>>11)&31U)*255U+15U)/31U);
        uint8_t g=(uint8_t)((((col>>5)&63U)*255U+31U)/63U);
        uint8_t b=(uint8_t)(((col&31U)*255U+15U)/31U);
        colors[i]=lv_color_make(r,g,b);
    }
    for(unsigned a=0;a<acts;a++){
        uint8_t info[4]={};
        if(!exact(file,info,4U))return false;
        unsigned id=info[0],w=info[1],h=info[2],count=info[3];
        if(id>11U || !w || !h || !count || count>24U)return false;
        uint32_t pos=(uint32_t)file.position();
        uint32_t length=(uint32_t)count*2U+(uint32_t)w*h*count;
        if(pos>size || length>size-pos)return false;
        if(id!=0U){
            if(!file.seek(pos+length))return false;
            continue;
        }
        if(w>kMaxSide || h>kMaxSide || count>12U)return false;
        size_t bytes=(size_t)w*h*kBytesPerPixel;
        if(!file.seek(pos+(uint32_t)count*2U))return false;
        uint8_t *p=(uint8_t *)heap_caps_malloc(bytes,
                                MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!p)return false;
        uint8_t row[kMaxSide];
        bool valid=true;
        for(unsigned y=0;y<h && valid;y++){
            if(!exact(file,row,w)){valid=false;break;}
            for(unsigned x=0;x<w;x++){
                uint8_t index=row[x];
                if(index!=255U && index>=palette_count){valid=false;break;}
                size_t offset=((size_t)y*w+x)*kBytesPerPixel;
                lv_color_t color=index==255U?lv_color_black():colors[index];
                memcpy(p+offset,&color,sizeof(color));
                p[offset+sizeof(color)]=index==255U?LV_OPA_TRANSP:LV_OPA_COVER;
            }
        }
        if(!valid){heap_caps_free(p);return false;}
        card.pixels=p;
        card.descriptor.header.always_zero=0U;
        card.descriptor.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;
        card.descriptor.header.w=w;
        card.descriptor.header.h=h;
        card.descriptor.data_size=bytes;
        card.descriptor.data=p;
        return true;
    }
    return false;
}
bool load_art(Card &card,uint16_t species,bool shiny,bool *real_shiny){
    if(real_shiny)*real_shiny=false;
    if(!mounted)return false;
    char path[24];
    if(shiny && sprite_path(species,true,path) && SD_MMC.exists(path)){
        File f=SD_MMC.open(path,FILE_READ);
        bool ok=f && !f.isDirectory() && decode_first_idle(f,card);
        if(f)f.close();
        if(ok){if(real_shiny)*real_shiny=true;return true;}
        forget(card);
    }
    if(!sprite_path(species,false,path))return false;
    File f=SD_MMC.open(path,FILE_READ);
    bool ok=f && !f.isDirectory() && decode_first_idle(f,card);
    if(f)f.close();
    return ok;
}
void draw_card(unsigned slot,size_t idx){
    Card &card=cards[slot];
    forget(card);
    if(idx>=participant_count){
        lv_obj_add_flag(card.root,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(card.root,LV_OBJ_FLAG_HIDDEN);
    const pk_pg_main_t &view=participants[idx];
    lv_label_set_text(card.name,view.name);
    if(!view.has_main){
        lv_label_set_text(card.species,"-");
        lv_label_set_text(card.status,"NOT LINKED");
        return;
    }
    const char *name=pk_lab_dex_name(view.species);
    char line[48];
    if(name){
        snprintf(line,sizeof(line),"%s #%u",name,(unsigned)view.species);
    }else{
        snprintf(line,sizeof(line),"POKEMON #%u",(unsigned)view.species);
    }
    lv_label_set_text(card.species,line);
    bool real_shiny=false;
    bool success=load_art(card,view.species,view.shiny!=0U,&real_shiny);
    if(success){
        lv_img_set_src(card.image,&card.descriptor);
        /* Zoom only; never increase LVGL object bounds (prevents tiling). */
        unsigned maxside=card.descriptor.header.w>card.descriptor.header.h?
            card.descriptor.header.w:card.descriptor.header.h;
        unsigned zoom=maxside?256U*60U/maxside:256U;
        if(zoom>512U)zoom=512U;
        lv_img_set_zoom(card.image,(uint16_t)zoom);
        lv_obj_align(card.image,LV_ALIGN_TOP_MID,0,
                     49-(int)card.descriptor.header.h/2);
    }
    /* The saved level comes from this player's real active profile.
     * Label is short enough for a 150px card on the circular display. */
    char status[48];
    if(view.shiny)
        snprintf(status,sizeof(status),real_shiny?"LV %u SHINY":"LV %u SHINY*",
                 (unsigned)view.level);
    else
        snprintf(status,sizeof(status),success?"LV %u MAIN":"LV %u NO ART",
                 (unsigned)view.level);
    lv_label_set_text(card.status,status);
}
void render(){
    unload_all();
    if(!screen)return;
    unsigned pages=(unsigned)((participant_count+kPage-1U)/kPage);
    char label[32];
    snprintf(label,sizeof(label),"PARCEIROS %u/%u",
             (unsigned)(page+1U),pages?pages:1U);
    lv_label_set_text(counter,label);
    /* One page: one centered BACK. Multiple pages: BACK + NEXT.
     * Both stay within the actual circular glass at y=300..330. */
    if(next_button){
        if(pages<=1U)
            lv_obj_add_flag(next_button,LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_clear_flag(next_button,LV_OBJ_FLAG_HIDDEN);
    }
    if(back_button){
        /* At y=336 the round glass permits only about x=90..270.
         * Compact buttons on multi-page screens remain fully inside. */
        if(pages<=1U){
            lv_obj_set_size(back_button,112,30);
            lv_obj_set_pos(back_button,124,306);
        }else{
            lv_obj_set_size(back_button,80,30);
            lv_obj_set_pos(back_button,90,306);
        }
    }
    if(next_button && pages>1U){
        lv_obj_set_size(next_button,80,30);
        lv_obj_set_pos(next_button,190,306);
    }
    int clk=-1,cmd=-1,d0=-1;
    if(board==&board_k518){clk=4;cmd=3;d0=5;}
    else if(board==&board_k718){clk=39;cmd=38;d0=40;}
    mounted=false;
    bool sd_started=clk>=0 && SD_MMC.setPins(clk,cmd,d0) &&
                    SD_MMC.begin("/sdcard",true,false);
    if(sd_started && SD_MMC.cardType()!=CARD_NONE)mounted=true;
    for(unsigned i=0;i<kPage;i++)draw_card(i,page*kPage+i);
    if(sd_started)SD_MMC.end();
    mounted=false;
}
void exit_view(lv_event_t *){
    unload_all();
    if(screen_pregame_roster)lv_scr_load(screen_pregame_roster);
}
void next_page(lv_event_t *){
    unsigned pages=(unsigned)((participant_count+kPage-1U)/kPage);
    if(pages<=1U)return;
    page=(page+1U)%pages;
    render();
}
lv_obj_t *make_label(lv_obj_t *parent,const char *s,int y,uint32_t rgb){
    lv_obj_t *o=lv_label_create(parent);
    lv_label_set_text(o,s);
    lv_obj_set_width(o,150);
    lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_font(o,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(o,lv_color_hex(rgb),0);
    lv_obj_align(o,LV_ALIGN_TOP_MID,0,y);
    return o;
}
lv_obj_t *make_button(const char *text,int x,int w,lv_event_cb_t cb){
    lv_obj_t *b=lv_btn_create(screen);
    lv_obj_set_size(b,w,30);
    lv_obj_set_pos(b,x,306);
    lv_obj_set_style_radius(b,15,0);
    lv_obj_set_style_bg_color(b,lv_color_hex(0xDFCC9F),0);
    lv_obj_set_style_shadow_width(b,0,0);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *label=lv_label_create(b);
    lv_label_set_text(label,text);
    lv_obj_set_style_text_color(label,lv_color_hex(0x132532),0);
    lv_obj_center(label);
    return b;
}
void build(){
    if(screen)return;
    screen=lv_obj_create(NULL);
    lv_obj_set_size(screen,360,360);
    lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_set_style_border_width(screen,0,0);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x091926),0);
    lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    counter=lv_label_create(screen);
    lv_label_set_text(counter,"PARCEIROS");
    lv_obj_set_style_text_font(counter,&lv_font_montserrat_22,0);
    lv_obj_set_style_text_color(counter,lv_color_hex(0xF5DDA7),0);
    lv_obj_align(counter,LV_ALIGN_TOP_MID,0,26);
    for(unsigned i=0;i<kPage;i++){
        int x=i%2U?185:25,y=i/2U?194:71;
        Card &c=cards[i];
        c.root=lv_obj_create(screen);
        lv_obj_set_pos(c.root,x,y);
        lv_obj_set_size(c.root,150,112);
        lv_obj_set_style_pad_all(c.root,0,0);
        lv_obj_set_style_radius(c.root,16,0);
        lv_obj_set_style_border_width(c.root,1,0);
        lv_obj_set_style_border_color(c.root,lv_color_hex(0x4F827E),0);
        lv_obj_set_style_bg_color(c.root,lv_color_hex(0x12313D),0);
        lv_obj_clear_flag(c.root,LV_OBJ_FLAG_SCROLLABLE);
        c.name=make_label(c.root,"PLAYER",4,0xF4E6C6);
        c.image=lv_img_create(c.root);
        lv_obj_align(c.image,LV_ALIGN_TOP_MID,0,19);
        c.species=make_label(c.root,"-",78,0xE4EFE9);
        c.status=make_label(c.root,"NOT LINKED",94,0x7DC3BB);
    }
    back_button=make_button("BACK",93,112,exit_view);
    next_button=make_button("NEXT >",215,85,next_page);
}
bool load_profile(void *,uint32_t user_id,pk_profile_t *out){
    if(!out)return false;
    pk_store_status_t status=pk_file_inspect(user_id,out);
    if(status==PK_STORE_VALID)return true;
    /* Only fall back to older NVS when no canonical SPIFFS record exists;
     * never override a corrupt record with potentially outdated history. */
    return status==PK_STORE_MISSING && pk_store_load(user_id,out);
}
} // namespace

extern "C" bool pk_pg_preview_open(const pk_pg_seat_t *seats,size_t count){
    if(!seats || count<1U || count>kSeats ||
       lv_scr_act()!=screen_pregame_roster)return false;
    pk_pg_main_t collected[kSeats]={};
    if(!pk_pg_collect_main(seats,count,load_profile,NULL,
                           collected,kSeats))return false;
    build();
    memcpy(participants,collected,count*sizeof(*collected));
    participant_count=count;
    page=0U;
    lv_scr_load(screen);
    render();
    return true;
}
