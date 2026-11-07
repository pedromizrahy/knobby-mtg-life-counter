#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_sprite_path.h"
int main(void)
{
    char path[24];
    assert(pk_sprite_path(778,path,sizeof(path)));
    assert(strcmp(path,"/mons/p778.bin")==0);
    assert(pk_sprite_path(1,path,sizeof(path)));
    assert(strcmp(path,"/mons/p1.bin")==0);
    assert(pk_sprite_path(1025,path,sizeof(path)));
    assert(strcmp(path,"/mons/p1025.bin")==0);
    assert(!pk_sprite_path(0,path,sizeof(path)));
    assert(!pk_sprite_path(1026,path,sizeof(path)));
    char tiny[5]="ZZZZ";
    assert(!pk_sprite_path(778,tiny,sizeof(tiny)));
    assert(tiny[4]=='\0');
    assert(!pk_sprite_path(778,NULL,0));
    assert(!pk_sprite_path(778,path,0));
    puts("Pokemon sprite path tests passed");
    return 0;
}
