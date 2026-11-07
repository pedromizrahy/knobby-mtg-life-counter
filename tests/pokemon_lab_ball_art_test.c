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
