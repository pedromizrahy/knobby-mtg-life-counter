#include "dice.h"
#include "dice_art.h"
#include "game.h"
#include "esp_random.h"
#include <stdio.h>

#define ITEM_COUNT 8
#define FRAME_MS 33
#define ROLL_FRAMES 36

static const int sides[ITEM_COUNT]={0,4,6,8,10,12,20,100};
static const char *names[ITEM_COUNT]={"COIN","D4","D6","D8","D10","D12","D20","D100"};
lv_obj_t *screen_dice=NULL;
static lv_obj_t *title=NULL,*hint=NULL,*clip=NULL,*picture=NULL,*face_number=NULL,*coin_fallback=NULL;
static lv_timer_t *roll_timer=NULL;
static int selected=0,target=0,last_result=-1;
static unsigned frame=0;
static bool rolling=false;
static int visible_coin=0;

static int random_result(int n)
{
    uint32_t r,limit;
    if (n==0) return (int)(esp_random() & 1U);
    limit=UINT32_MAX-(UINT32_MAX % (uint32_t)n);
    do { r=esp_random(); } while(r>=limit);
    return (int)(r % (uint32_t)n)+1;
}
static void set_art(int art)
{
    const lv_img_dsc_t *src=dial_art_get(art);
    if(src) {
        lv_img_set_src(picture,src);
        lv_obj_clear_flag(picture,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(coin_fallback,LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(picture,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(coin_fallback,LV_OBJ_FLAG_HIDDEN);
    }
}
static void set_face(int number)
{
    char buf[16];
    if(selected==0) {
        lv_obj_add_flag(face_number,LV_OBJ_FLAG_HIDDEN);
        set_art(visible_coin);
        lv_label_set_text(coin_fallback,visible_coin==0?"HEADS":"TAILS");
    } else {
        set_art(selected+1);
        if(number>0) {
            snprintf(buf,sizeof(buf),"%d",number);
            lv_label_set_text(face_number,buf);
            lv_obj_clear_flag(face_number,LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(face_number);
        } else {
            lv_obj_add_flag(face_number,LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(coin_fallback,names[selected]);
    }
}
void refresh_dice_ui(void)
{
    if(!title) return;
    lv_label_set_text(title,names[selected]);
    set_face(last_result);
}
void dice_change_selection(int delta)
{
    if(rolling || delta==0) return;
    selected=(selected+(delta>0?1:-1)+ITEM_COUNT)%ITEM_COUNT;
    last_result=-1;
    visible_coin=0;
    lv_obj_set_width(clip,188);
    lv_img_set_angle(picture,0);
    lv_img_set_zoom(picture,256);
    refresh_dice_ui();
}
static void animate(lv_timer_t *timer)
{
    int current;
    (void)timer;
    if(!rolling || lv_scr_act()!=screen_dice) {
        rolling=false;
        lv_timer_pause(roll_timer);
        return;
    }
    frame++;
    if(selected==0) {
        /* Fold the visible image into an edge and swap sides between flips. */
        unsigned phase=(frame*8U)%ROLL_FRAMES;
        int edge=(int)(phase <= ROLL_FRAMES/2U ? phase : ROLL_FRAMES-phase);
        lv_obj_set_width(clip,188-(edge*174)/(ROLL_FRAMES/2));
        visible_coin=(int)((frame*8U/ROLL_FRAMES)&1U);
        if(frame>=ROLL_FRAMES) visible_coin=target;
        set_face(-1);
    } else {
        current=(frame>=ROLL_FRAMES)?target:random_result(sides[selected]);
        /* Keep numeral on the die, not outside it; hide it while tumbling,
         * then reveal the destination face near the end. */
        lv_img_set_angle(picture,(int16_t)((frame*231U)%3600U));
        lv_img_set_zoom(picture,(uint16_t)(230U+((frame%7U)*6U)));
        set_face(frame>=ROLL_FRAMES-5U?target:current);
    }
    if(frame>=ROLL_FRAMES) {
        rolling=false;
        last_result=target;
        visible_coin=target;
        lv_obj_set_width(clip,188);
        lv_img_set_angle(picture,0);
        lv_img_set_zoom(picture,256);
        set_face(target);
        lv_label_set_text(hint,"Turn dial to select  |  Tap to roll");
        lv_timer_pause(roll_timer);
        printf("[Dice] %s => %d\n",names[selected],target);
    }
}
void dice_roll_selected(void)
{
    if(rolling || !roll_timer) return;
    target=random_result(sides[selected]);
    frame=0;
    rolling=true;
    last_result=-1;
    lv_label_set_text(hint,"Rolling...");
    lv_timer_reset(roll_timer);
    lv_timer_resume(roll_timer);
}
void open_dice_screen(void)
{
    if(!screen_dice) return;
    if(roll_timer) lv_timer_pause(roll_timer);
    rolling=false;
    selected=0;target=0;last_result=-1;visible_coin=0;
    lv_obj_set_width(clip,188);
    lv_img_set_angle(picture,0);
    lv_img_set_zoom(picture,256);
    lv_label_set_text(hint,"Turn dial to select  |  Tap to roll");
    refresh_dice_ui();
    load_screen_if_needed(screen_dice);
}
static void clicked(lv_event_t *e) { (void)e; dice_roll_selected(); }
void event_tool_dice(lv_event_t *e) { (void)e; open_dice_screen(); }

void build_dice_screen(void)
{
    screen_dice=lv_obj_create(NULL);
    lv_obj_set_size(screen_dice,360,360);
    lv_obj_set_style_bg_color(screen_dice,lv_color_black(),0);
    lv_obj_set_style_border_width(screen_dice,0,0);
    lv_obj_set_style_pad_all(screen_dice,0,0);
    lv_obj_set_scrollbar_mode(screen_dice,LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(screen_dice,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_dice,clicked,LV_EVENT_CLICKED,NULL);

    title=lv_label_create(screen_dice);
    lv_obj_set_style_text_color(title,lv_color_hex(0xF5C653),0);
    lv_obj_set_style_text_font(title,&lv_font_montserrat_32,0);
    lv_obj_align(title,LV_ALIGN_TOP_MID,0,45);

    clip=lv_obj_create(screen_dice);
    lv_obj_set_size(clip,188,188);
    lv_obj_set_style_bg_opa(clip,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(clip,0,0);
    lv_obj_set_style_pad_all(clip,0,0);
    lv_obj_set_scrollbar_mode(clip,LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(clip,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(clip,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_align(clip,LV_ALIGN_CENTER,0,0);

    picture=lv_img_create(clip);
    lv_obj_center(picture);
    lv_obj_add_flag(picture,LV_OBJ_FLAG_EVENT_BUBBLE);

    coin_fallback=lv_label_create(clip);
    lv_obj_set_style_text_color(coin_fallback,lv_color_white(),0);
    lv_obj_set_style_text_font(coin_fallback,&lv_font_montserrat_32,0);
    lv_obj_center(coin_fallback);
    lv_obj_add_flag(coin_fallback,LV_OBJ_FLAG_EVENT_BUBBLE);

    face_number=lv_label_create(screen_dice);
    lv_obj_set_style_text_color(face_number,lv_color_white(),0);
    lv_obj_set_style_text_font(face_number,&lv_font_montserrat_32,0);
    lv_obj_align(face_number,LV_ALIGN_CENTER,0,-1);
    lv_obj_add_flag(face_number,LV_OBJ_FLAG_EVENT_BUBBLE);

    hint=lv_label_create(screen_dice);
    lv_obj_set_style_text_color(hint,lv_color_hex(0x999999),0);
    lv_obj_set_style_text_font(hint,&lv_font_montserrat_14,0);
    lv_obj_align(hint,LV_ALIGN_BOTTOM_MID,0,-39);

    roll_timer=lv_timer_create(animate,FRAME_MS,NULL);
    lv_timer_pause(roll_timer);
    refresh_dice_ui();
}
