#include "pokemon_lab_ball_art.h"
#include <string.h>
/* Hand-drawn procedural pixel art. One native RGB565+alpha frame, no file
 * access. Transparent 96px canvas gives the lid space to rise safely. */
#define S ((int)PK_LAB_BALL_PIXELS)
static const uint16_t top[4]={0xD965U,0x295FU,0x2104U,0x981FU};
static const uint16_t lite[4]={0xFA08U,0x6D5FU,0xFEA0U,0xDA3FU};
static const uint16_t shade[4]={0xA082U,0x10B7U,0x1082U,0x6818U};
static const uint16_t flare[4]={0xFFDFU,0xB6FFU,0xFFE0U,0xE77FU};
static void px(uint8_t *buf,int x,int y,uint16_t c) {
    if(x<0 || x>=S || y<0 || y>=S)return;
    size_t n=((size_t)y*PK_LAB_BALL_PIXELS+(size_t)x)*3U;
    /* LV_COLOR_DEPTH=16 and LV_COLOR_16_SWAP=1 on the DIAL. */
    buf[n]=(uint8_t)(c>>8);buf[n+1]=(uint8_t)c;buf[n+2]=255;
}
static void circle(uint8_t *b,int cx,int cy,int r,uint16_t c) {
    for(int y=cy-r;y<=cy+r;y++)for(int x=cx-r;x<=cx+r;x++){
        int dx=x-cx,dy=y-cy;
        if(dx*dx+dy*dy<=r*r)px(b,x,y,c);
    }
}
static void cross(uint8_t *b,int x,int y,int radius,uint16_t color) {
    for(int d=-radius;d<=radius;d++){
        px(b,x+d,y,color); px(b,x,y+d,color);
    }
    px(b,x-1,y-1,0xFFFFU);
}
static uint16_t shell_top(uint8_t ball,int dx,int dy,int rr){
    if(rr>=643)return 0x08A2U;
    if(rr>=585)return shade[ball];
    if(dx>13 || dy> -2)return shade[ball];
    if(dx< -12 && dy< -9)return lite[ball];
    return top[ball];
}
static uint16_t shell_bottom(int dx,int dy,int rr){
    if(rr>=643)return 0x08A2U;
    if(rr>=585 || (dy>17 && dx>7))return 0xAE34U;
    if(dy>14)return 0xCE18U;
    if(dx< -12 && dy<14)return 0xFFFFU;
    return 0xEF7DU;
}
static void insignia(uint8_t *buf,uint8_t ball,int cx,int cy,int lift) {
    if(ball==1U) {
        for(int y=cy-13;y<=cy-8;y++)for(int x=cx-21;x<=cx-14;x++)px(buf,x,y-lift,0xE986U);
        for(int y=cy-13;y<=cy-8;y++)for(int x=cx+14;x<=cx+21;x++)px(buf,x,y-lift,0xE986U);
    } else if(ball==2U) {
        for(int a=0;a<11;a++){
            px(buf,cx-10+a,cy-21+a/3-lift,0xFEA0U);
            px(buf,cx+10-a,cy-21+a/3-lift,0xFEA0U);
        }
    } else if(ball==3U) {
        circle(buf,cx-15,cy-13-lift,3,0xF81FU);
        circle(buf,cx+15,cy-13-lift,3,0xF81FU);
        for(int i=0;i<7;i++){
            px(buf,cx-7,cy-21+i-lift,0xFFFFU);
            px(buf,cx+7,cy-21+i-lift,0xFFFFU);
            px(buf,cx-7+i,cy-21+i/2-lift,0xFFFFU);
            px(buf,cx+7-i,cy-21+i/2-lift,0xFFFFU);
        }
    }
}
bool pk_lab_ball_art_render(uint8_t ball,unsigned frame,uint8_t *dst,size_t capacity){
    if(ball>=4U || frame>=PK_LAB_BALL_FRAMES || !dst ||
       capacity<PK_LAB_BALL_BUFFER_BYTES)return false;
    memset(dst,0,PK_LAB_BALL_BUFFER_BYTES);
    static const int shake[8]={0,0,0,-2,3,-3,2,0};
    static const int lift[18]={0,0,0,0,0,0,0,0,0,0,0,0,0,2,4,7,10,13};
    int cx=48+shake[frame<8U?frame:7U],cy=52,hinge=lift[frame];
    bool opening=frame>=13U;
    /* Ground shadow is oval; outside the drawing stays transparent. */
    for(int y=79;y<=81;y++)for(int x=24;x<=72;x++){
        int dx=x-48,dy=(y-80)*5;
        if(dx*dx+dy*dy<485)px(dst,x,y,0x2128U);
    }
    /* Restrained outline glow BEHIND the ball, never a solid square. */
    if(frame>=11U){
        unsigned r=frame>=15U?31U:28U;
        for(int y=cy-(int)r;y<=cy+(int)r;y++)for(int x=cx-(int)r;x<=cx+(int)r;x++){
            int dx=x-cx,dy=y-cy,d=dx*dx+dy*dy;
            if(d>=(int)(r-2)*(int)(r-2) && d<=(int)r*(int)r && ((x+y)&1)==0)
                px(dst,x,y,flare[ball]);
        }
        cross(dst,19,24,frame>=15U?3:1,flare[ball]);
        cross(dst,77,25,frame>=15U?3:1,flare[ball]);
        if(frame>=15U){cross(dst,15,64,2,lite[ball]);cross(dst,81,65,2,lite[ball]);}
    }
    /* Ivory bowl with shaded underside and outlined perimeter. */
    for(int y=cy;y<=cy+26;y++)for(int x=cx-26;x<=cx+26;x++){
        int dx=x-cx,dy=y-cy,rr=dx*dx+dy*dy;
        if(rr<=676)px(dst,x,y,shell_bottom(dx,dy,rr));
    }
    for(int x=cx-23;x<=cx+23;x++){
        int dx=x-cx;
        if(dx*dx<=529){px(dst,x,cy,0x08A2U);if(opening)px(dst,x,cy+1,0x67BFU);}
    }
    if(opening){
        /* Light leaks through the growing gap; a clean tapered beam rather
         * than the old checkerboard fills the space between both halves. */
        for(int y=cy-hinge+2;y<cy;y++){
            int reach=12-(y-(cy-hinge+2))/4;
            for(int x=cx-reach;x<=cx+reach;x++){
                int dx=x-cx;
                px(dst,x,y,dx>=-2 && dx<=2?0xFFFFU:flare[ball]);
            }
        }
    }
    /* The cap remains fully inside the canvas through the final frame:
     * at the maximum lift its highest visible pixel is at y=13. */
    for(int y=cy-26;y<=cy;y++)for(int x=cx-26;x<=cx+26;x++){
        int dx=x-cx,dy=y-cy,rr=dx*dx+dy*dy;
        if(rr<=676)px(dst,x,y-hinge,shell_top(ball,dx,dy,rr));
    }
    for(int y=cy-19;y<=cy-16;y++)for(int x=cx-17;x<=cx-10;x++){
        int dx=x-cx,dy=y-cy;
        if(dx*dx+dy*dy<580)px(dst,x,y-hinge,lite[ball]);
    }
    insignia(dst,ball,cx,cy,hinge);
    if(!opening){
        for(int y=cy-3;y<=cy+3;y++)for(int x=cx-24;x<=cx+24;x++){
            int dx=x-cx,dy=y-cy;
            if(dx*dx+dy*dy<=660)px(dst,x,y,0x08A2U);
        }
        circle(dst,cx,cy,8,0x08A2U);
        circle(dst,cx,cy,6,0xAD75U);
        circle(dst,cx,cy,4,frame>=9U?flare[ball]:0xF7DEU);
        px(dst,cx-2,cy-2,0xFFFFU);
    }else{
        circle(dst,cx,cy+1,5,0x08A2U);
        circle(dst,cx,cy+1,3,flare[ball]);
    }
    return true;
}
