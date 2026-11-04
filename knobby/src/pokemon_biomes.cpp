#include "pokemon_biomes.h"
#include <Arduino.h>
#include "../board_detect.h"
#include <FS.h>
#include <SD_MMC.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr int W=180,H=180;
lv_color_t *canvas=nullptr;
lv_img_dsc_t image={};
uint32_t seed=0x778F00D;
uint32_t noise(int x,int y,int salt=0) {
    uint32_t v=(uint32_t)x*374761393u+(uint32_t)y*668265263u+
                (uint32_t)salt*2246822519u+seed;
    v=(v^(v>>13))*1274126177u;
    return v^(v>>16);
}
void pixel(int x,int y,uint32_t c) {
    if((unsigned)x<W && (unsigned)y<H)canvas[y*W+x]=lv_color_hex(c);
}
void rect(int x,int y,int w,int h,uint32_t c) {
    for(int yy=y<0?0:y;yy<y+h && yy<H;++yy)
      for(int xx=x<0?0:x;xx<x+w && xx<W;++xx)pixel(xx,yy,c);
}
void disk(int cx,int cy,int rx,int ry,uint32_t c) {
    if(!rx||!ry)return;
    for(int y=cy-ry;y<=cy+ry;++y)
      for(int x=cx-rx;x<=cx+rx;++x) {
        long dx=x-cx,dy=y-cy;
        if(dx*dx*ry*ry+dy*dy*rx*rx<=
           (long)rx*rx*ry*ry)pixel(x,y,c);
      }
}
void triangle(int cx,int top,int halfwidth,int height,uint32_t c) {
    for(int dy=0;dy<height;++dy) {
        int span=halfwidth*dy/height;
        rect(cx-span,top+dy,span*2+1,1,c);
    }
}
void stepped_hills(int y,int amplitude,uint32_t top,uint32_t bottom,int salt) {
    for(int x=0;x<W;++x) {
        int crest=y+((int)(noise(x/14,0,salt)%7)-3)*amplitude/3+
                  ((int)(noise(x/34,2,salt)%7)-3)*amplitude/4;
        rect(x,crest,1,H-crest,top);
        if(crest+6<H)rect(x,crest+6,1,H-crest-6,bottom);
    }
}
void tree(int x,int y,int size,bool dark=false) {
    uint32_t trunk=dark?0x29283D:0x715237;
    uint32_t shadow=dark?0x343E54:0x236548;
    uint32_t crown=dark?0x5B6279:0x389867;
    rect(x-2,y-size/3,4,size/2,trunk);
    disk(x,y-size/2,size/2,size/3,shadow);
    disk(x-5,y-size/2-5,size/3,size/4,crown);
    disk(x+5,y-size/2-9,size/3,size/4,crown);
    rect(x-7,y-size/2-8,5,3,dark?0x747A8A:0x65B77A);
}
void flower(int x,int y,uint32_t c) {
    pixel(x,y-2,c);pixel(x-2,y,c);pixel(x+2,y,c);pixel(x,y+2,c);
    pixel(x,y,0xFFE9A0);rect(x,y+3,1,4,0x277B4B);
}
void rock(int x,int y,int size,uint32_t dark,uint32_t light) {
    disk(x,y,size,size/2+1,dark);
    rect(x-size/2,y-size/3,size,2,light);
    rect(x-size/2,y-size/3+2,2,size/3,light);
}
void cloud(int x,int y) {
    rect(x-16,y,34,6,0xF2F8ED);
    rect(x-10,y-5,12,7,0xF2F8ED);
    rect(x+3,y-9,12,12,0xFFFFFF);
}
void sky(uint32_t top,uint32_t mid,uint32_t bottom,int horizon) {
    for(int y=0;y<H;++y) {
        uint32_t col=y<horizon/3?top:(y<horizon*2/3?mid:bottom);
        rect(0,y,W,1,col);
    }
}
void ground_detail(int start,uint32_t a,uint32_t b,int salt) {
    for(int y=start;y<H;y+=5)for(int x=0;x<W;x+=6) {
        int xx=x+(int)(noise(x,y,salt)%5);
        int yy=y+(int)(noise(y,x,salt)%4);
        rect(xx,yy,2,1,(noise(xx,yy,salt)&1)?a:b);
    }
}
void meadow() {
    sky(0x76C9ED,0x9ADAEF,0xC4EAD8,112);
    cloud(48,37);cloud(139,52);
    disk(136,32,12,12,0xFFE5A4);
    stepped_hills(93,10,0x8BCC79,0x5EB16F,3);
    stepped_hills(125,8,0x55AC65,0x4B984E,9);
    ground_detail(128,0x70C86C,0x2F854E,12);
    tree(17,134,39);tree(155,128,33);
    for(int i=0;i<23;++i) {
        int x=(int)(noise(i,7)%168)+6,y=133+(int)(noise(i,9)%42);
        flower(x,y,(i%3==0)?0xFFD8E9:(i%3==1?0xE9F9FF:0xF9E6AB));
    }
}
void forest() {
    sky(0x335D72,0x4B8E8C,0x95C8A4,122);
    disk(141,32,12,12,0xFAEAC4);
    stepped_hills(105,5,0x367957,0x285E48,11);
    ground_detail(121,0x487D47,0x629757,13);
    for(int i=0;i<8;++i) {
        int x=8+i*25+(int)(noise(i,2)%8)-4;
        tree(x,114+(int)(noise(i,4)%14),45+(int)(noise(i,6)%12));
    }
    for(int i=0;i<7;++i) {
        int x=(int)(noise(i,21)%174),y=146+(int)(noise(i,24)%27);
        rock(x,y,3,0x374F4B,0x87A995);
    }
}
void sea() {
    sky(0x70BDEB,0xA6DFF5,0xE5EFD6,94);
    cloud(39,38);cloud(148,28);
    disk(132,42,15,15,0xFFE5A4);
    rect(0,94,W,58,0x2698C3);
    rect(0,98,W,6,0x4DBFDA);
    for(int j=0;j<6;++j) {
        int y=108+j*8;
        for(int i=0;i<12;++i) {
            int x=(i*19+(j%2)*9);
            rect(x,y+(int)(noise(i,j,31)%3),9,2,(j&1)?0x8DDDE3:0x4FB5CC);
        }
    }
    for(int x=0;x<W;++x) {
        int shore=149+(int)(noise(x/10,2,25)%5);
        rect(x,shore,1,H-shore,0xEBCB8C);
        pixel(x,shore-2,0xF2F5D8);pixel(x,shore-1,0xB5E7D7);
    }
    ground_detail(156,0xEEDFA7,0xC8A56C,48);
    rock(23,152,8,0x71898A,0xA8C9BE);
    rock(163,169,6,0x827D71,0xC4A998);
}
void volcano() {
    sky(0x49394D,0x874D50,0xE2955A,125);
    disk(143,43,16,14,0xE8A16D);
    triangle(88,47,77,107,0x4B444B);
    triangle(88,65,60,92,0x665057);
    triangle(38,98,55,72,0x403D45);
    triangle(151,107,45,64,0x39383F);
    triangle(88,47,21,20,0x272D34);
    rect(78,55,18,7,0xFF8E41);
    rect(83,62,9,59,0xF78B3D);
    rect(84,68,5,45,0xFFD26B);
    rect(0,149,W,31,0x3C3B43);
    for(int i=0;i<8;++i) {
        int x=(int)(noise(i,14)%166),y=140+(int)(noise(i,15)%36);
        rect(x,y,12,3,0xCA573E);rect(x+2,y,7,2,0xFF9C4A);
    }
    for(int i=0;i<24;++i)pixel((int)(noise(i,3)%W),
                               36+(int)(noise(i,5)%116),0xFFC17A);
    rock(28,161,8,0x31343A,0x77717B);
    rock(143,165,11,0x35363C,0x6D6269);
}
void cave() {
    sky(0x111D37,0x1C3150,0x324762,180);
    rect(0,0,W,28,0x263140);
    for(int x=0;x<W;x+=14)triangle(x,21+(int)(noise(x,3)%9),7,
                                      12+(int)(noise(x,5)%18),0x1C283B);
    rect(0,145,W,35,0x353C4C);
    for(int i=0;i<14;++i) {
        int x=(int)(noise(i,8)%180),y=135+(int)(noise(i,10)%45);
        rock(x,y,5+(int)(noise(i,13)%6),0x242D42,0x627183);
    }
    for(int i=0;i<7;++i) {
        int x=13+(int)(noise(i,18)%155),y=137+(int)(noise(i,19)%28);
        triangle(x,y,4,10+(int)(noise(i,21)%15),0x58A7C5);
        triangle(x,y+4,2,7,0xB5EFF6);
    }
    for(int i=0;i<19;++i)pixel((int)(noise(i,12)%W),
                               25+(int)(noise(i,14)%105),0x657989);
}
void haunted() {
    sky(0x181D3C,0x303457,0x4A506C,115);
    disk(136,42,22,22,0xE8E9C9);
    disk(147,36,20,22,0x303457);
    stepped_hills(118,11,0x3A4658,0x273744,3);
    ground_detail(128,0x46575A,0x303E46,61);
    for(int i=0;i<7;++i) {
        int x=5+i*27+(int)(noise(i,35)%8);
        tree(x,134+(int)(noise(i,40)%20),34,true);
    }
    for(int i=0;i<5;++i) {
        int x=22+i*32,y=146+(int)(noise(i,12)%25);
        rect(x,y,10,13,0x8B94A2);
        disk(x+5,y,5,5,0x8B94A2);
        rect(x+4,y+4,2,9,0x5D687A);
        rect(x+1,y+8,8,2,0x5D687A);
    }
    rect(0,173,W,7,0x283642);
    for(int i=0;i<8;++i) {
        int x=(int)(noise(i,61)%W),y=115+(int)(noise(i,62)%54);
        rect(x,y,4,2,0x94A5AA);
    }
}

/* Optional licensed/custom scenery on microSD:
 * /scenes/pokemon/{meadow,forest,sea,volcano,cave,haunted}.pkbg.
 * Exact format: PKBG + little-endian 180,180 + 32400 RGB565 little-endian pixels.
 * Convert each pixel via lv_color_make to respect LV_COLOR_16_SWAP.
 * Never create, delete or write an SD file. */
static const char *const scene_paths[PK_BIOME_COUNT]={
    "/scenes/pokemon/meadow.pkbg",
    "/scenes/pokemon/forest.pkbg",
    "/scenes/pokemon/sea.pkbg",
    "/scenes/pokemon/volcano.pkbg",
    "/scenes/pokemon/cave.pkbg",
    "/scenes/pokemon/haunted.pkbg"
};
bool load_external_scene(unsigned biome) {
    int clk=-1,cmd=-1,d0=-1;
    if(board==&board_k518) {clk=4;cmd=3;d0=5;}
    else if(board==&board_k718) {clk=39;cmd=38;d0=40;}
    else return false;
    if(!SD_MMC.setPins(clk,cmd,d0) ||
       !SD_MMC.begin("/sdcard",true,false)) return false;
    bool ok=false;
    const char *path=scene_paths[biome];
    if(SD_MMC.cardType()!=CARD_NONE && SD_MMC.exists(path)) {
        File file=SD_MMC.open(path,FILE_READ);
        if(file && !file.isDirectory() && file.size()==8UL+W*H*2UL) {
            uint8_t header[8];
            if(file.read(header,sizeof(header))==sizeof(header) &&
               memcmp(header,"PKBG",4)==0 &&
               header[4]==(W&255) && header[5]==(W>>8) &&
               header[6]==(H&255) && header[7]==(H>>8)) {
                ok=true;
                /* Read at most one scanline at a time; no large stack buffer. */
                uint8_t scanline[W*2];
                for(int y=0;y<H && ok;++y) {
                    if(file.read(scanline,sizeof(scanline))!=sizeof(scanline)) {
                        ok=false;
                        break;
                    }
                    for(int x=0;x<W;++x) {
                        uint16_t rgb=(uint16_t)scanline[x*2] |
                                     ((uint16_t)scanline[x*2+1]<<8);
                        uint8_t r=(uint8_t)((((rgb>>11)&31)*255+15)/31);
                        uint8_t g=(uint8_t)((((rgb>>5)&63)*255+31)/63);
                        uint8_t b=(uint8_t)(((rgb&31)*255+15)/31);
                        canvas[y*W+x]=lv_color_make(r,g,b);
                    }
                }
            }
        }
        if(file)file.close();
    }
    SD_MMC.end();
    if(ok)Serial.printf("[Pokemon] BIOME external asset id=%u (read-only)\\n",biome);
    return ok;
}
} // namespace

extern "C" void pk_biome_release(void) {
    free(canvas);
    canvas=nullptr;
    memset(&image,0,sizeof(image));
}
extern "C" bool pk_biome_render(unsigned biome) {
    if(biome>=PK_BIOME_COUNT)return false;
    if(!canvas)canvas=(lv_color_t *)malloc((size_t)W*H*sizeof(lv_color_t));
    if(!canvas) {
        Serial.println("[Pokemon] BIOME ERR: 180x180 pixel canvas alloc failed");
        return false;
    }
    seed=0x778F00D;
    bool external=load_external_scene(biome);
    if(!external) switch(biome) {
        case PK_BIOME_MEADOW: meadow();break;
        case PK_BIOME_FOREST: forest();break;
        case PK_BIOME_SEA: sea();break;
        case PK_BIOME_VOLCANO: volcano();break;
        case PK_BIOME_CAVE: cave();break;
        case PK_BIOME_HAUNTED: haunted();break;
    }
    image.header.always_zero=0;
    image.header.w=W;
    image.header.h=H;
    image.header.cf=LV_IMG_CF_TRUE_COLOR;
    image.data=(const uint8_t *)canvas;
    image.data_size=(size_t)W*H*sizeof(lv_color_t);
    Serial.printf("[Pokemon] BIOME READY id=%u 180x180 pixel-art\n",biome);
    return true;
}
extern "C" const lv_img_dsc_t *pk_biome_image(void) {
    return canvas?&image:nullptr;
}
