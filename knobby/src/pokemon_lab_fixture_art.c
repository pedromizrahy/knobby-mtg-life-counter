#include "pokemon_lab_fixture_art.h"
#include <string.h>
static void dot(uint8_t *b,int x,int y,uint16_t c)
{
    if(x<0 || y<0 || x>=40 || y>=40)return;
    size_t i=((size_t)y*40U+(size_t)x)*3U;
    b[i]=(uint8_t)c;b[i+1]=(uint8_t)(c>>8);b[i+2]=255;
}
static void line(uint8_t *b,int x0,int y0,int x1,int y1,uint16_t c,int w)
{
    int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1;
    int dy=y1>y0?y0-y1:y1-y0,sy=y0<y1?1:-1;
    int error=dx+dy;
    for(;;){
        for(int yy=-w;yy<=w;yy++)
            for(int xx=-w;xx<=w;xx++)
                dot(b,x0+xx,y0+yy,c);
        if(x0==x1 && y0==y1)break;
        int e2=error*2;
        if(e2>=dy){error+=dy;x0+=sx;}
        if(e2<=dx){error+=dx;y0+=sy;}
    }
}
static void circle(uint8_t *b,int cx,int cy,int r,uint16_t c,bool filled)
{
    for(int y=0;y<40;y++)for(int x=0;x<40;x++){
        int dx=x-cx,dy=y-cy,d=dx*dx+dy*dy;
        if(filled?d<=r*r:(d>=r*r-r*2 && d<=r*r+r*2))
            dot(b,x,y,c);
    }
}
bool pk_lab_fixture_art(uint16_t species,bool shiny,uint8_t *out,size_t capacity)
{
    if(!out || capacity<PK_LAB_FIXTURE_BUFFER_BYTES)return false;
    unsigned tier=0;
    if(species==25)tier=0;
    else if(species==778)tier=1;
    else if(species==149)tier=2;
    else if(species==150)tier=3;
    else return false;
    static const uint16_t base[4]={0x4C6EU,0x4D9FU,0x8C1BU,0xC40FU};
    static const uint16_t bright[4]={0xFEE0U,0x7E7FU,0xD37FU,0xF6B7U};
    const uint16_t color=shiny?0xFFDFU:bright[tier];
    memset(out,0,PK_LAB_FIXTURE_BUFFER_BYTES);
    /* Crest and four thematic motifs (bolt / ghost / wing / star).
     * Abstract laboratory markers; not licensed character sprites. */
    circle(out,20,20,18,base[tier],true);
    circle(out,20,20,17,0x0841U,false);
    if(tier==0){
        line(out,24,7,14,21,color,1);
        line(out,14,21,21,21,color,1);
        line(out,21,21,15,33,color,1);
    }else if(tier==1){
        circle(out,20,17,10,color,true);
        line(out,10,17,10,30,color,2);
        line(out,30,17,30,30,color,2);
        line(out,10,30,15,26,color,2);
        line(out,15,26,20,30,color,2);
        line(out,20,30,25,26,color,2);
        line(out,25,26,30,30,color,2);
        dot(out,16,17,0x0841U);dot(out,24,17,0x0841U);
    }else if(tier==2){
        line(out,20,10,20,32,color,1);
        line(out,20,19,7,9,color,1);
        line(out,20,19,33,9,color,1);
        line(out,7,9,13,25,color,1);
        line(out,33,9,27,25,color,1);
        line(out,13,25,20,19,color,1);
        line(out,27,25,20,19,color,1);
    }else{
        circle(out,20,20,11,color,false);
        line(out,20,8,20,32,color,1);
        line(out,8,20,32,20,color,1);
        line(out,11,11,29,29,color,0);
        line(out,29,11,11,29,color,0);
        circle(out,20,20,3,0xFFFFU,true);
    }
    if(shiny){
        dot(out,5,5,0xFFFFU);dot(out,34,5,0xFFFFU);
        dot(out,5,34,0xFFFFU);dot(out,34,34,0xFFFFU);
    }
    return true;
}
