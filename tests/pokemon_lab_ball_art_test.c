#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_lab_ball_art.h"
int main(void){
    uint8_t buffer[PK_LAB_BALL_BUFFER_BYTES];
    memset(buffer,0x77,sizeof(buffer));
    assert(!pk_lab_ball_art_render(4,0,buffer,sizeof(buffer)));
    assert(buffer[0]==0x77);
    assert(!pk_lab_ball_art_render(0,PK_LAB_BALL_FRAMES,buffer,sizeof(buffer)));
    assert(!pk_lab_ball_art_render(0,0,buffer,sizeof(buffer)-1));
    assert(!pk_lab_ball_art_render(0,0,NULL,sizeof(buffer)));
    unsigned total=0;
    for(unsigned ball=0;ball<4;ball++)
        for(unsigned frame=0;frame<PK_LAB_BALL_FRAMES;frame++){
            assert(pk_lab_ball_art_render((uint8_t)ball,frame,buffer,sizeof(buffer)));
            unsigned visible=0,transparent=0;
            for(unsigned i=2;i<sizeof(buffer);i+=3){
                assert(buffer[i]==0 || buffer[i]==255);
                if(buffer[i]==255)visible++;else transparent++;
            }
            assert(visible>60 && transparent>100);
            total+=visible;
        }
    uint8_t first[PK_LAB_BALL_BUFFER_BYTES],other[PK_LAB_BALL_BUFFER_BYTES];
    assert(pk_lab_ball_art_render(0,0,first,sizeof(first)));
    assert(pk_lab_ball_art_render(1,0,other,sizeof(other)));
    assert(memcmp(first,other,sizeof(first))!=0);
    assert(pk_lab_ball_art_render(0,PK_LAB_BALL_FRAMES-1,other,sizeof(other)));
    assert(memcmp(first,other,sizeof(first))!=0);
    assert(total>5000);
    /* No opaque square around the Pokeball, and the lid never leaves
     * the 96px image even in the last frame. */
    for(unsigned ball=0;ball<4;ball++){
        assert(pk_lab_ball_art_render((uint8_t)ball,
                    PK_LAB_BALL_FRAMES-1,buffer,sizeof(buffer)));
        assert(buffer[2]==0U);
        assert(buffer[((PK_LAB_BALL_PIXELS-1U)*
                      PK_LAB_BALL_PIXELS+PK_LAB_BALL_PIXELS-1U)*3U+2U]==0U);
        unsigned lid_pixels=0;
        for(unsigned y=10;y<24;y++)
            for(unsigned x=24;x<72;x++)
                if(buffer[(y*PK_LAB_BALL_PIXELS+x)*3U+2U]==255U)
                    lid_pixels++;
        assert(lid_pixels>50U);
    }
    /* The pixel-art animation has distinct shake, latch, and reveal poses;
     * each frame must fit its fixed 64x64 RGB565+alpha buffer. */
    uint8_t shake[PK_LAB_BALL_BUFFER_BYTES];
    uint8_t open[PK_LAB_BALL_BUFFER_BYTES];
    assert(pk_lab_ball_art_render(0,4,shake,sizeof(shake)));
    assert(pk_lab_ball_art_render(0,15,open,sizeof(open)));
    assert(memcmp(shake,open,sizeof(shake))!=0);
    puts("Pokemon procedural pixel-ball animation tests passed");
    return 0;
}
