#include "pokemon_lab_ball_art.h"
#include "pokemon_progress.h"
#include <string.h>
/* Four original 32px pixel balls. Their color schemes are lab previews,
 * not official sprites or graphical promises for the final game. */
/* RGB565 tones: Poke red, Great blue, Ultra dark, Master PURPLE. */
static const uint16_t tops[4]={0xD965U,0x295FU,0x2104U,0x981FU};
static const uint16_t gleams[4]={0xFBCFU,0xEA29U,0xFEC0U,0xF83FU};
static void pixel(uint8_t *dst,int x,int y,uint16_t color)
{
    if(x<0 || x>=32 || y<0 || y>=32)return;
    for(unsigned sy=0;sy<2;sy++)
        for(unsigned sx=0;sx<2;sx++){
            const unsigned pos=((unsigned)(2*y+sy)*64U+
                                (unsigned)(2*x+sx))*3U;
            /* Firmware LV_COLOR_16_SWAP=1: native LVGL color must be swapped. */
            dst[pos]=(uint8_t)(color>>8);
            dst[pos+1]=(uint8_t)color;
            dst[pos+2]=255U;
        }
}
static void disc(uint8_t *dst,int cx,int cy,int r,uint16_t color)
{
    for(int y=0;y<32;y++)
        for(int x=0;x<32;x++){
            int dx=x-cx,dy=y-cy;
            if(dx*dx+dy*dy<=r*r)
                pixel(dst,x,y,color);
        }
}
bool pk_lab_ball_art_render(uint8_t ball,unsigned frame,
                            uint8_t *dst,size_t cap)
{
    if(ball>=4U || frame>=PK_LAB_BALL_FRAMES || !dst ||
       cap<PK_LAB_BALL_BUFFER_BYTES)return false;
    memset(dst,0,PK_LAB_BALL_BUFFER_BYTES);
    /* Base shell, black outline with ivory lower semicircle. */
    for(int y=17;y<32;y++)
        for(int x=0;x<32;x++){
            int dx=x-16,dy=y-17;
            int r2=dx*dx+dy*dy;
            if(r2<=144)
                pixel(dst,x,y,r2>=110?0x0841U:0xF7BEU);
        }
    int lift=frame>=2U?(int)(frame-1U)*2:0;
    int shake=frame==1U?1:0;
    /* Top half rises, exposing the open lower shell. */
    for(int y=0;y<=17;y++)
        for(int x=0;x<32;x++){
            int dx=x-16,dy=y-17;
            int r2=dx*dx+dy*dy;
            if(r2>144)continue;
            uint16_t color=r2>=110?0x0841U:tops[ball];
            if(y>=15 && frame<=1U)color=0x0841U;
            if(x>=10 && x<=12 && y>=8 && y<=10 && r2<110)
                color=gleams[ball];
            if(ball==PK_BALL_GREAT && y>=8 && y<=12 &&
               ((x>=6 && x<=10)||(x>=22 && x<=26)))color=0xD965U;
            if(ball==PK_BALL_ULTRA && y>=7 && y<=12 &&
               (x==12 || x==13 || x==19 || x==20))color=0xFEC0U;
            if(ball==PK_BALL_MASTER && y>=8 && y<=12 &&
               ((x>=8 && x<=10)||(x>=22 && x<=24)))color=0xF83FU;
            pixel(dst,x+shake,y-lift,color);
        }
    /* Black hinge/band, central clasp with a crisp pixel border. */
    if(frame<=1U){
        for(int y=16;y<=18;y++)
            for(int x=5;x<=27;x++)
                if((x-16)*(x-16)+(y-17)*(y-17)<=144)
                    pixel(dst,x+shake,y,0x0841U);
        disc(dst,16+shake,17,4,0x0841U);
        disc(dst,16+shake,17,2,0xFFDFU);
        pixel(dst,15+shake,16,0xFFFFU);
    }else{
        for(int x=8;x<=24;x++)
            pixel(dst,x,17,0x0841U);
        disc(dst,16,18,3,0x0841U);
        disc(dst,16,18,1,0xFFDFU);
        if(frame>=3U){
            /* Simple pixel sparkles, no allocations per frame. */
            pixel(dst,16,10,0xFFFFU);
            pixel(dst,15,11,0xFFFFU);
            pixel(dst,17,11,0xFFFFU);
            if(frame>=4U){
                pixel(dst,7,10,gleams[ball]);
                pixel(dst,25,7,gleams[ball]);
                pixel(dst,24,15,0xFFFFU);
            }
        }
    }
    return true;
}
