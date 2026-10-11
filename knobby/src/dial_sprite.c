#include "dial_sprite.h"
#include "esp_heap_caps.h"
#include <stdint.h>
#include <string.h>

#if __has_include("dial_coin_frames.h")
#include "dial_coin_frames.h"
#define HAVE_COIN 1
#else
#define HAVE_COIN 0
#endif
#if __has_include("dial_intro_asset.h")
#include "dial_intro_asset.h"
#define HAVE_INTRO 1
#else
#define HAVE_INTRO 0
#endif
#if __has_include("dial_d4_frames.h")
#include "dial_d4_frames.h"
#define HAVE_D4 1
#else
#define HAVE_D4 0
#endif
#if __has_include("dial_d6_frames.h")
#include "dial_d6_frames.h"
#define HAVE_D6 1
#else
#define HAVE_D6 0
#endif
#if __has_include("dial_d8_frames.h")
#include "dial_d8_frames.h"
#define HAVE_D8 1
#else
#define HAVE_D8 0
#endif
#if __has_include("dial_d10_frames.h")
#include "dial_d10_frames.h"
#define HAVE_D10 1
#else
#define HAVE_D10 0
#endif
#if __has_include("dial_d100t_frames.h") && __has_include("dial_d100u_frames.h")
#include "dial_d100t_frames.h"
#include "dial_d100u_frames.h"
#define HAVE_D100 1
#else
#define HAVE_D100 0
#endif
#if __has_include("dial_d12_frames.h")
#include "dial_d12_frames.h"
#define HAVE_D12 1
#else
#define HAVE_D12 0
#endif

typedef struct {
    uint16_t width, height, roll_count, total;
    const uint8_t *data;
    const uint32_t *offsets;
    const uint32_t *lengths;
} dial_frames_t;
#define FRAMES(prefix) {prefix##_WIDTH,prefix##_HEIGHT,prefix##_ROLL_COUNT,prefix##_TOTAL,prefix##_stream,prefix##_offsets,prefix##_lengths}
static const dial_frames_t *source_for(int kind)
{
    switch(kind) {
#if HAVE_COIN
        case 0: { static const dial_frames_t f=FRAMES(dial_coin); return &f; }
#endif
#if HAVE_D4
        case 1: { static const dial_frames_t f=FRAMES(dial_d4); return &f; }
#endif
#if HAVE_D6
        case 2: { static const dial_frames_t f=FRAMES(dial_d6); return &f; }
#endif
#if HAVE_D8
        case 3: { static const dial_frames_t f=FRAMES(dial_d8); return &f; }
#endif
#if HAVE_D10
        case 4: { static const dial_frames_t f=FRAMES(dial_d10); return &f; }
#endif
#if HAVE_D100
        case 7: { static const dial_frames_t f=FRAMES(dial_d100t); return &f; }
#endif
#if HAVE_D12
        case 5: { static const dial_frames_t f=FRAMES(dial_d12); return &f; }
#endif
#if HAVE_INTRO
        case 99: { static const dial_frames_t f=FRAMES(dial_intro); return &f; }
#endif
        default: return NULL;
    }
}
bool dial_sprite_available(int kind) { return source_for(kind)!=NULL; }

static const dial_frames_t *source_units_for_d100(void)
{
#if HAVE_D100
    static const dial_frames_t f=FRAMES(dial_d100u);
    return &f;
#else
    return NULL;
#endif
}
static lv_obj_t *images[2]={NULL,NULL};
static lv_img_dsc_t descriptors[2];
static uint8_t *pixel_buffers[2]={NULL,NULL};
static lv_obj_t *owner[2]={NULL,NULL};
#define MAX_SPRITE_BYTES (272U*272U*2U)

void dial_sprite_hide(void)
{
    for(int i=0;i<2;i++)
        if(images[i]) lv_obj_add_flag(images[i],LV_OBJ_FLAG_HIDDEN);
}

static bool ensure_slot(lv_obj_t *parent,int slot)
{
    if(!pixel_buffers[slot]) {
        pixel_buffers[slot]=(uint8_t *)heap_caps_malloc(MAX_SPRITE_BYTES,
                         MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!pixel_buffers[slot]) return false;
    }
    if(!images[slot] || owner[slot]!=parent) {
        if(images[slot]) lv_obj_del(images[slot]);
        images[slot]=lv_img_create(parent);
        lv_obj_add_flag(images[slot],LV_OBJ_FLAG_EVENT_BUBBLE);
        owner[slot]=parent;
    }
    return true;
}
static void render_slot(lv_obj_t *parent,int slot,const dial_frames_t *f,
                        unsigned index,int x,int y)
{
    uint32_t pixels=(uint32_t)f->width*f->height;
    if(index>=f->total || pixels*2U>MAX_SPRITE_BYTES ||
       !ensure_slot(parent,slot)) return;
    uint32_t pos=f->offsets[index];
    const uint32_t end=pos+f->lengths[index];
    uint32_t at=0;
    uint8_t *dst=pixel_buffers[slot];
    while(pos+2U<end && at<pixels) {
        unsigned run=f->data[pos++];
        uint8_t lo=f->data[pos++],hi=f->data[pos++];
        while(run-- && at<pixels) {
            /* LV_COLOR_16_SWAP == 1, as in the board's lv_conf.h. */
            dst[2U*at]=hi; dst[2U*at+1U]=lo; at++;
        }
    }
    if(at!=pixels) return;
    lv_img_dsc_t *d=&descriptors[slot];
    lv_img_cache_invalidate_src(d);
    memset(d,0,sizeof(*d));
    d->header.always_zero=0;
    d->header.w=f->width;
    d->header.h=f->height;
    d->header.cf=LV_IMG_CF_TRUE_COLOR;
    d->data_size=pixels*2U;
    d->data=dst;
    lv_img_set_src(images[slot],d);
    lv_obj_align(images[slot],LV_ALIGN_CENTER,x,y);
    lv_obj_clear_flag(images[slot],LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(images[slot]);
}
void dial_sprite_show(lv_obj_t *parent,int kind,unsigned frame,int result)
{
    const dial_frames_t *f=source_for(kind);
    if(!parent||!f) return;
    unsigned idx;
    if(kind==99) idx=0;
    else if(frame<f->roll_count) idx=frame;
    else if(kind==0) idx=f->roll_count+(result==1?1U:0U);
    else {
        int max=(int)(f->total-f->roll_count);
        int value=result;
        if(kind==7) value=(result==100?0:result/10)+1;
        if(value<1||value>max)value=1;
        idx=f->roll_count+(unsigned)(value-1);
    }
    render_slot(parent,0,f,idx,kind==99?-0:(kind==7?-51:0),kind==99?-44:0);
    if(kind==7) {
        const dial_frames_t *units_frame=source_units_for_d100();
        int units=result%10;
        unsigned second=(frame<f->roll_count)?frame:
                         f->roll_count+(unsigned)units;
        render_slot(parent,1,units_frame,second,51,0);
        /* A pair of 184px sprites must be reduced to avoid overlap. */
        if(images[0]) lv_img_set_zoom(images[0],170);
        if(images[1]) lv_img_set_zoom(images[1],170);
    } else {
        if(images[0]) lv_img_set_zoom(images[0],256);
        if(images[1]) lv_obj_add_flag(images[1],LV_OBJ_FLAG_HIDDEN);
    }
}
