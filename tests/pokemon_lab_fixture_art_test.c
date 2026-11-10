#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../knobby/src/pokemon_lab_fixture_art.h"
int main(void){
    uint8_t sample[PK_LAB_FIXTURE_BUFFER_BYTES];
    memset(sample,0xAB,sizeof(sample));
    assert(!pk_lab_fixture_art(999,false,sample,sizeof(sample)));
    assert(sample[0]==0xAB);
    assert(!pk_lab_fixture_art(25,false,sample,sizeof(sample)-1));
    assert(!pk_lab_fixture_art(25,false,NULL,sizeof(sample)));
    const unsigned ids[]={25,778,149,150};
    uint8_t first[PK_LAB_FIXTURE_BUFFER_BYTES];
    for(unsigned i=0;i<4;i++){
        assert(pk_lab_fixture_art((uint16_t)ids[i],false,sample,sizeof(sample)));
        unsigned opaque=0;
        for(unsigned k=2;k<sizeof(sample);k+=3){
            assert(sample[k]==0 || sample[k]==255);
            if(sample[k])opaque++;
        }
        assert(opaque>500 && opaque<1600);
        if(i==0)memcpy(first,sample,sizeof(first));
        if(i)assert(memcmp(first,sample,sizeof(first))!=0);
        uint8_t shiny[PK_LAB_FIXTURE_BUFFER_BYTES];
        assert(pk_lab_fixture_art((uint16_t)ids[i],true,shiny,sizeof(shiny)));
        assert(memcmp(shiny,sample,sizeof(shiny))!=0);
    }
    puts("Pokemon LAB synthetic emblem tests passed");
    return 0;
}
