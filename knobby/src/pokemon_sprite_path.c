#include "pokemon_sprite_path.h"
#include <stdio.h>
bool pk_sprite_path(uint16_t species,char *buffer,size_t capacity)
{
    if(!buffer || species<1U || species>1025U || capacity==0U)return false;
    const int n=snprintf(buffer,capacity,"/mons/p%u.bin",(unsigned)species);
    return n>0 && (size_t)n<capacity;
}
