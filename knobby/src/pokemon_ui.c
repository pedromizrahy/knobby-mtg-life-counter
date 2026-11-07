#include "pokemon_ui.h"
#include "pokemon_lab_ui.h"
#include "pokemon_tpk2_renderer.h"
#include "pokemon_companion_selection.h"
#include "pokemon_companion_profile_view.h"
#include "pokemon_profile_store.h"
#include "pokemon_biomes.h"
#include <stdio.h>
#include <stdint.h>

/* A companion-focused 360x360 display. Art always comes from user-installed
 * TPK2 sprites on SD. No synthetic rewards or Playgroup numbers.
 * Explicit other-species selection is opt-in; Mimikyu #778 remains default.
 * All controls operate outside Magic gameplay screens. */
lv_obj_t *screen_pokemon_companion = NULL;

static lv_obj_t *biome_background = NULL;
static lv_obj_t *mimikyu_image = NULL;
static lv_obj_t *mimikyu_status = NULL;
static lv_obj_t *theme_label = NULL;
static lv_obj_t *page_label = NULL;
static lv_obj_t *species_label = NULL;
static lv_obj_t *species_types = NULL;
static lv_obj_t *level_label = NULL;
static uint16_t displayed_species = 0;
/* Only an ephemeral preview choice, NOT ownership or a profile write. */
static uint16_t preview_species = 778U;
static const uint16_t kPreviewCandidates[]={778U,25U,149U,150U};
static lv_obj_t *header_plate = NULL;
static lv_obj_t *info_panel = NULL;
static lv_obj_t *stats_panel = NULL;
static lv_timer_t *mimikyu_timer = NULL;
static unsigned mimikyu_frame = 0;
static unsigned companion_page = 0;
static unsigned scene_theme = 0;

/* Biome selection is session-local; no NVS or gameplay-state writes. */
static const char *const kSceneNames[PK_BIOME_COUNT]={
    "MEADOW","FOREST","SEA","VOLCANO","CAVE","HAUNTED"
};
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
    lv_obj_set_size(btn,width,32);
    lv_obj_align(btn,LV_ALIGN_TOP_MID,x,278);
    lv_obj_set_style_radius(btn,16,0);
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
    if(pk_biome_render(scene_theme)) {
        lv_obj_clear_flag(biome_background,LV_OBJ_FLAG_HIDDEN);
        /* The image descriptor is reused for each scene; invalidate LVGL
         * cached decode before swapping the underlying pixels. */
        lv_img_cache_invalidate_src(pk_biome_image());
        lv_img_set_src(biome_background,pk_biome_image());
        lv_img_set_zoom(biome_background,512); /* 180px pixel art -> 360px */
        lv_obj_center(biome_background);
    } else {
        lv_obj_add_flag(biome_background,LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(theme_label,kSceneNames[scene_theme]);
}
static void update_page(void)
{
    bool home=companion_page==0;
    if(home) {
        lv_obj_clear_flag(info_panel,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(stats_panel,LV_OBJ_FLAG_HIDDEN);
        if (pk_tpk2_idle_frame_count())
            lv_obj_clear_flag(mimikyu_image,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(mimikyu_image,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(page_label,"PARTNER PREVIEW");
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
static void browse_preview(int step)
{
    if(lv_scr_act()!=screen_pokemon_companion || companion_page!=0)return;
    unsigned index=0;
    for(unsigned i=0;i<4;i++)
        if(kPreviewCandidates[i]==preview_species)index=i;
    for(unsigned n=0;n<4;n++){
        index=(index+(step>0?1U:3U))%4U;
        const uint16_t candidate=kPreviewCandidates[index];
        if(pokemon_ui_show_species(candidate)){
            preview_species=candidate;
            return;
        }
    }
    /* An SD sprite is required, otherwise no companion is invented. */
}
static void on_preview_prev(lv_event_t *event)
{
    (void)event;
    browse_preview(-1);
}
static void on_preview_next(lv_event_t *event)
{
    (void)event;
    browse_preview(1);
}
void pokemon_ui_restore_preview(void)
{
    if(!pokemon_ui_show_species(preview_species)){
        preview_species=778U;
        (void)pokemon_ui_show_species(778U);
    }
}
static void on_lab(lv_event_t *event)
{
    (void)event;
    pokemon_lab_ui_open();
}
static void on_theme(lv_event_t *event)
{
    (void)event;
    scene_theme=(scene_theme+1U)%PK_BIOME_COUNT;
    update_theme();
}
static void idle_tick(lv_timer_t *timer)
{
    (void)timer;
    if(lv_scr_act()!=screen_pokemon_companion || companion_page!=0 ||
       !mimikyu_image || pk_tpk2_idle_frame_count()==0) return;
    mimikyu_frame=(mimikyu_frame+1U)%pk_tpk2_idle_frame_count();
    lv_img_set_src(mimikyu_image,pk_tpk2_idle_frame(mimikyu_frame));
    lv_obj_align(mimikyu_image,LV_ALIGN_CENTER,0,-7);
    lv_timer_set_period(mimikyu_timer,pk_tpk2_idle_duration_ms(mimikyu_frame));
}
void pokemon_ui_knob_page(int direction)
{
    if(lv_scr_act()!=screen_pokemon_companion || direction==0)return;
    companion_page=(companion_page+1U)%2U;
    update_page();
}
/* Switch sprites only on the UI thread, after detaching the LVGL image.
 * The renderer reuses a bounded PSRAM frame cache, so the previous cache
 * must never remain referenced by a visible LVGL object when released.
 * This is a preview API, not an inventory selection or a reward claim. */
bool pokemon_ui_show_species(uint16_t species)
{
    if(!screen_pokemon_companion || !mimikyu_image || !mimikyu_timer ||
       species<1U || species>1025U)
        return false;
    if(pk_tpk2_idle_loaded_species()==species &&
       pk_tpk2_idle_frame_count()>0) {
        displayed_species=species;
    } else {
        const uint16_t previous=pk_tpk2_idle_loaded_species();
        lv_timer_pause(mimikyu_timer);
        lv_obj_add_flag(mimikyu_image,LV_OBJ_FLAG_HIDDEN);
        /* Drop the old descriptor while memory is still allocated. */
        lv_img_set_src(mimikyu_image,NULL);
        for(unsigned i=0;i<pk_tpk2_idle_frame_count();i++)
            lv_img_cache_invalidate_src(pk_tpk2_idle_frame(i));

        bool ready=pk_tpk2_idle_load_species(species);
        if(!ready) {
            /* A failed test selection does not invent a companion.
             * Restore the last valid species, or the Mimikyu default. */
            if(previous && previous!=species)
                ready=pk_tpk2_idle_load_species(previous);
            if(!ready && species!=778U && previous!=778U)
                ready=pk_tpk2_idle_load_species(778U);
            if(!ready) {
                displayed_species=0;
                if(species_label)lv_label_set_text(species_label,"SPRITE UNAVAILABLE");
                if(species_types)lv_label_set_text(species_types,"TYPE: NOT AVAILABLE");
                update_page();
                return false;
            }
            displayed_species=pk_tpk2_idle_loaded_species();
        } else displayed_species=species;
    }
    if(species_label) {
        if(displayed_species==778U)lv_label_set_text(species_label,"MIMIKYU  #778");
        else {
            char label[32];
            snprintf(label,sizeof(label),"POKEMON  #%u",(unsigned)displayed_species);
            lv_label_set_text(species_label,label);
        }
    }
    if(species_types)
        lv_label_set_text(species_types,
                          displayed_species==778U?"GHOST  /  FAIRY":"TYPE: NOT LINKED");
    if(level_label)lv_label_set_text(level_label,"LV / XP: NOT LINKED");
    mimikyu_frame=0;
    lv_img_set_src(mimikyu_image,pk_tpk2_idle_frame(0));
    lv_obj_align(mimikyu_image,LV_ALIGN_CENTER,0,-7);
    lv_timer_set_period(mimikyu_timer,pk_tpk2_idle_duration_ms(0));
    lv_timer_reset(mimikyu_timer);
    lv_timer_resume(mimikyu_timer);
    update_page();
    return displayed_species==species;
}
bool pokemon_ui_show_owned_species(const pk_profile_t *profile,
                                    uint16_t species)
{
    if(!pk_companion_is_owned(profile,species))return false;
    return pokemon_ui_show_species(species);
}
static bool load_saved_profile(void *ctx,uint32_t id,pk_profile_t *out)
{
    (void)ctx;
    return pk_store_load(id,out);
}
bool pokemon_ui_show_saved_species(uint32_t verified_user_id,
                                   uint16_t species)
{
    /* This opt-in read must never touch SD/LVGL on a Magic gameplay
     * screen. The current user must already be verified by the caller. */
    if(!screen_pokemon_companion ||
       lv_scr_act()!=screen_pokemon_companion)return false;
    pk_companion_view_t view;
    if(!pk_companion_read_view(load_saved_profile,NULL,
                               verified_user_id,species,&view))
        return false;
    if(!pokemon_ui_show_species(view.species))return false;
    if(level_label){
        char label[40];
        snprintf(label,sizeof(label),"LV %u  /  XP %lu",
                 (unsigned)view.level,(unsigned long)view.xp);
        lv_label_set_text(level_label,label);
    }
    return true;
}
void pokemon_ui_open_companion(void)
{
    companion_page=0;
    /* Keep the preview choice in RAM; default to Mimikyu #778. */
    pokemon_ui_restore_preview();
    update_theme();
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

    /* Backdrop is a full-screen original pixel-art bitmap drawn at x2. */
    biome_background=lv_img_create(screen_pokemon_companion);
    lv_obj_clear_flag(biome_background,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(biome_background,LV_ALIGN_CENTER,0,0);

    /* Readable high-contrast header over every bright/dark biome. */
    header_plate=scene_shape(screen_pokemon_companion,260,62,50,24,20);
    lv_obj_set_style_bg_color(header_plate,lv_color_hex(0x172D35),0);
    lv_obj_set_style_bg_opa(header_plate,LV_OPA_90,0);
    page_label=text_label(header_plate,"COMPANION",5,
                          &lv_font_montserrat_14,0xB4E8CC);
    species_label=text_label(header_plate,"MIMIKYU  #778",23,
                             &lv_font_montserrat_22,0xFFFFFF);
    theme_label=text_label(screen_pokemon_companion,"MEADOW",91,
                           &lv_font_montserrat_14,0x173B37);
    lv_obj_set_style_bg_color(theme_label,lv_color_hex(0xF1EEDB),0);
    lv_obj_set_style_bg_opa(theme_label,LV_OPA_90,0);

    mimikyu_image=lv_img_create(screen_pokemon_companion);
    lv_obj_align(mimikyu_image,LV_ALIGN_CENTER,0,-7);
    mimikyu_status=text_label(screen_pokemon_companion,"SPRITE LOAD FAILED - CHECK SD",165,
                              &lv_font_montserrat_14,0xFFFFFF);
    lv_obj_set_style_bg_color(mimikyu_status,lv_color_hex(0x48252C),0);
    lv_obj_set_style_bg_opa(mimikyu_status,LV_OPA_90,0);

    info_panel=scene_shape(screen_pokemon_companion,232,39,64,236,17);
    lv_obj_set_style_bg_opa(info_panel,LV_OPA_90,0);
    species_types=text_label(info_panel,"GHOST  /  FAIRY",2,
                             &lv_font_montserrat_14,0x274F46);
    /* Real level/XP requires a connected profile; never invent stats. */
    level_label=text_label(info_panel,"LV / XP: NOT LINKED",20,
                              &lv_font_montserrat_14,0x274F46);
    lv_obj_set_width(level_label,225);

    stats_panel=scene_shape(screen_pokemon_companion,276,164,42,115,20);
    lv_obj_set_style_bg_opa(stats_panel,LV_OPA_90,0);
    text_label(stats_panel,"GAME STATISTICS",18,&lv_font_montserrat_16,0xF0E7C4);
    text_label(stats_panel,"Games   --       Wins   --",56,
               &lv_font_montserrat_14,0xE8F0E4);
    text_label(stats_panel,"Streak  --       Damage  --",85,
               &lv_font_montserrat_14,0xE8F0E4);
    text_label(stats_panel,"Playgroup not linked",112,
               &lv_font_montserrat_14,0xE8F0E4);
    /* Test lab is an opt-in screen; never a Playgroup player/account. */
    lv_obj_t *lab_entry=lv_btn_create(stats_panel);
    lv_obj_set_size(lab_entry,104,29);
    lv_obj_align(lab_entry,LV_ALIGN_BOTTOM_MID,0,-3);
    lv_obj_set_style_radius(lab_entry,14,0);
    lv_obj_set_style_bg_color(lab_entry,lv_color_hex(0xEAD6A4),0);
    lv_obj_add_event_cb(lab_entry,on_lab,LV_EVENT_CLICKED,NULL);
    lv_obj_t *lab_caption=lv_label_create(lab_entry);
    lv_label_set_text(lab_caption,"TEST LAB");
    lv_obj_set_style_text_color(lab_caption,lv_color_hex(0x19303A),0);
    lv_obj_set_style_text_font(lab_caption,&lv_font_montserrat_14,0);
    lv_obj_center(lab_caption);

    soft_button(screen_pokemon_companion,"BACK",68,-82,on_back);
    /* Preview only installed TPK2, never pretend an unowned Pokemon is in team. */
    lv_obj_t *prev=lv_btn_create(screen_pokemon_companion);
    lv_obj_set_pos(prev,108,317);
    lv_obj_set_size(prev,67,28);
    lv_obj_set_style_radius(prev,14,0);
    lv_obj_set_style_bg_color(prev,lv_color_hex(0xF6F1DE),0);
    lv_obj_add_event_cb(prev,on_preview_prev,LV_EVENT_CLICKED,NULL);
    lv_obj_t *prev_text=lv_label_create(prev);
    lv_label_set_text(prev_text,"<");
    lv_obj_set_style_text_color(prev_text,lv_color_hex(0x152735),0);
    lv_obj_center(prev_text);
    lv_obj_t *next=lv_btn_create(screen_pokemon_companion);
    lv_obj_set_pos(next,185,317);
    lv_obj_set_size(next,67,28);
    lv_obj_set_style_radius(next,14,0);
    lv_obj_set_style_bg_color(next,lv_color_hex(0xF6F1DE),0);
    lv_obj_add_event_cb(next,on_preview_next,LV_EVENT_CLICKED,NULL);
    lv_obj_t *next_text=lv_label_create(next);
    lv_label_set_text(next_text,">");
    lv_obj_set_style_text_color(next_text,lv_color_hex(0x152735),0);
    lv_obj_center(next_text);
    soft_button(screen_pokemon_companion,"SCENE",74,0,on_theme);
    soft_button(screen_pokemon_companion,"STATS",68,82,on_page);

    pokemon_lab_ui_build();
    mimikyu_timer=lv_timer_create(idle_tick,200,NULL);
    lv_timer_pause(mimikyu_timer);
    /* Scene allocation is deferred until entry to reserve heap for sprite. */
    lv_obj_add_flag(biome_background,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(theme_label,kSceneNames[scene_theme]);
    update_page();
}
