#include "pokemon_lab_ball_art.h"
#include <string.h>

/* Original hand-coded, 64px RGB565/alpha pixel scene. Single illustration
 * centered on a dark transparent canvas; no sprites or external assets.
 * The larger logical grid avoids the old chunky 32->64->192 upscale. */
static const uint16_t shell[4]={0xD965U,0x295FU,0x2104U,0x981FU};
static const uint16_t bright[4]={0xFB4FU,0x7C3FU,0xFEC0U,0xF83FU};
static const uint16_t dark[4]={0x8802U,0x1095U,0x1082U,0x6017U};
static const uint16_t halo[4]={0xF3E6U,0x8C5FU,0xFEA6U,0xDE7FU};
#define SIDE ((int)PK_LAB_BALL_PIXELS)

static void put(uint8_t *dst,int x,int y,uint16_t color)
{
    if(x<0 || x>=SIDE || y<0 || y>=SIDE)return;
    const unsigned offset=((unsigned)y*PK_LAB_BALL_PIXELS+(unsigned)x)*3U;
    /* LV_COLOR_DEPTH=16, LV_COLOR_16_SWAP=1. */
    dst[offset]=(uint8_t)(color>>8);
    dst[offset+1U]=(uint8_t)color;
    dst[offset+2U]=255U;
}
static void disk(uint8_t *dst,int cx,int cy,int radius,uint16_t color)
{
    for(int y=cy-radius;y<=cy+radius;y++)
        for(int x=cx-radius;x<=cx+radius;x++){
            const int dx=x-cx,dy=y-cy;
            if(dx*dx+dy*dy<=radius*radius)put(dst,x,y,color);
        }
}
static void line(uint8_t *dst,int x0,int y0,int x1,int y1,uint16_t color)
{
    int dx=x1>x0?x1-x0:x0-x1;
    int dy=y1>y0?y1-y0:y0-y1;
    int sx=x0<x1?1:-1, sy=y0<y1?1:-1,err=dx-dy;
    for(;;){
        put(dst,x0,y0,color);
        if(x0==x1 && y0==y1)break;
        const int e=2*err;
        if(e> -dy){err-=dy;x0+=sx;}
        if(e<dx){err+=dx;y0+=sy;}
    }
}
static void spark(uint8_t *dst,int x,int y,int size,uint16_t color)
{
    line(dst,x-size,y,x+size,y,color);
    line(dst,x,y-size,x,y+size,color);
    put(dst,x-1,y-1,0xFFFFU);
}
bool pk_lab_ball_art_render(uint8_t ball,unsigned frame,
                            uint8_t *dst,size_t cap)
{
    if(ball>=4U || frame>=PK_LAB_BALL_FRAMES || !dst ||
       cap<PK_LAB_BALL_BUFFER_BYTES)return false;
    memset(dst,0,PK_LAB_BALL_BUFFER_BYTES);
    const int shakes[4]={-3,3,-2,2};
    const int cx=32+(frame>=3U && frame<=6U?shakes[frame-3U]:0);
    const int cy=33;
    const int lift=frame>=10U?(int)(frame-9U)*3:0;
    const bool opened=frame>=10U;
    const uint16_t glow=halo[ball];

    /* Shadow grounds the moving ball. The last few frames create a radiant
     * opening without a framebuffer-size temporary allocation. */
    for(int y=54;y<=57;y++)
        for(int x=15;x<=49;x++){
            const int dx=x-32,dy=(y-55)*3;
            if(dx*dx+dy*dy<260)put(dst,x,y,0x2128U);
        }
    if(frame>=8U){
        const int reach=frame>=15U?30:frame>=12U?27:24;
        if(frame>=11U){
            line(dst,32,30,32,32-reach,glow);
            line(dst,32,33,32,33+reach,glow);
            line(dst,29,33,32-reach,33,glow);
            line(dst,35,33,32+reach,33,glow);
            line(dst,29,30,32-reach/2,33-reach/2,glow);
            line(dst,35,30,32+reach/2,33-reach/2,glow);
        }
        spark(dst,10,12,frame>=14U?3:1,glow);
        spark(dst,54,16,frame>=14U?3:1,glow);
        if(frame>=14U){
            spark(dst,12,47,2,bright[ball]);
            spark(dst,51,48,2,bright[ball]);
        }
    }
    /* Lower ivory hemisphere: outlined arc, warm inner core, darker lower
     * shading. The cap moves independently rather than a sudden jump. */
    for(int y=cy;y<=cy+21;y++)
        for(int x=cx-21;x<=cx+21;x++){
            const int dx=x-cx,dy=y-cy,r2=dx*dx+dy*dy;
            if(r2>441)continue;
            uint16_t color=r2>=382?0x1082U:y>=48?0xC5D5U:0xF7DEU;
            if(dx< -10 && y<46)color=0xFFFFU;
            put(dst,x,y,color);
        }
    if(opened){
        /* Interior light shining between the separated upper and lower
         * shells; only visible after the latch disengages. */
        for(int y=cy-lift+3;y<cy+1;y++)
            for(int x=29;x<=35;x++)
                put(dst,x,y,(x==32)?0xFFFFU:glow);
    }
    for(int y=cy-21;y<=cy;y++)
        for(int x=cx-21;x<=cx+21;x++){
            const int dx=x-cx,dy=y-cy,r2=dx*dx+dy*dy;
            if(r2>441)continue;
            uint16_t color=r2>=382?0x1082U:
                           (dx>=10 || dy> -2)?dark[ball]:shell[ball];
            if(dy< -7 && dx< -6 && r2<300)color=shell[ball];
            put(dst,x,y-lift,color);
        }
    /* Pale glint and signature accents unique to each type of ball. */
    for(int y=17;y<=20;y++)
        for(int x=22;x<=27;x++)
            if((x-22)*(x-22)+(y-20)*(y-20)<33)
                put(dst,x+(cx-32),y-lift,bright[ball]);
    if(ball==1U){
        for(int y=23;y<=28;y++){
            for(int x=16;x<=21;x++)put(dst,x+(cx-32),y-lift,0xE986U);
            for(int x=43;x<=48;x++)put(dst,x+(cx-32),y-lift,0xE986U);
        }
    }else if(ball==2U){
        for(int i=0;i<9;i++){
            put(dst,24+i+(cx-32),19+i/2-lift,0xFEC0U);
            put(dst,40-i+(cx-32),19+i/2-lift,0xFEC0U);
        }
    }else if(ball==3U){
        disk(dst,21+(cx-32),23-lift,3,0xF83FU);
        disk(dst,43+(cx-32),23-lift,3,0xF83FU);
        line(dst,28+(cx-32),19-lift,28+(cx-32),25-lift,0xFFFFU);
        line(dst,28+(cx-32),19-lift,32+(cx-32),23-lift,0xFFFFU);
        line(dst,32+(cx-32),23-lift,36+(cx-32),19-lift,0xFFFFU);
        line(dst,36+(cx-32),19-lift,36+(cx-32),25-lift,0xFFFFU);
    }
    if(!opened){
        for(int y=31;y<=35;y++)
            for(int x=cx-19;x<=cx+19;x++){
                const int dx=x-cx,dy=y-cy;
                if(dx*dx+dy*dy<=420)put(dst,x,y,0x1082U);
            }
        disk(dst,cx,cy,7,0x1082U);
        disk(dst,cx,cy,5,frame>=7U?glow:0xD69AU);
        disk(dst,cx,cy,3,frame>=8U?0xFFFFU:0xF7DEU);
        put(dst,cx-2,cy-2,0xFFFFU);
    }else{
        line(dst,22,33,42,33,0x1082U);
        disk(dst,32,34,4,0x1082U);
        disk(dst,32,34,2,0xFFFFU);
    }
    return true;
}
