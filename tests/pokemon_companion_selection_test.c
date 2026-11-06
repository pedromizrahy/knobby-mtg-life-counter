#include <assert.h>
#include <stdio.h>
#include "../knobby/src/pokemon_companion_selection.h"
int main(void)
{
    pk_profile_t profile;
    pk_profile_init(&profile);
    assert(!pk_companion_is_owned(&profile,778));
    assert(pk_companion_next_owned(&profile,0,1)==0);
    assert(pk_team_add(&profile,778,false));
    assert(pk_companion_is_owned(&profile,778));
    assert(!pk_companion_is_owned(&profile,25));
    assert(pk_companion_next_owned(&profile,0,1)==778);
    assert(pk_companion_next_owned(&profile,0,-1)==778);
    assert(pk_companion_next_owned(&profile,778,1)==778);
    assert(pk_register_discovery(&profile,25,false));
    assert(pk_register_discovery(&profile,1025,true));
    assert(pk_companion_is_owned(&profile,1025));
    assert(pk_companion_next_owned(&profile,25,1)==778);
    assert(pk_companion_next_owned(&profile,778,1)==1025);
    assert(pk_companion_next_owned(&profile,1025,1)==25);
    assert(pk_companion_next_owned(&profile,25,-1)==1025);
    assert(pk_companion_next_owned(&profile,0,-1)==1025);
    assert(pk_companion_next_owned(&profile,1025,-1)==778);
    assert(pk_companion_next_owned(&profile,1026,1)==0);
    assert(pk_companion_next_owned(&profile,25,0)==0);
    profile.version=2;
    assert(!pk_companion_is_owned(&profile,778));
    assert(pk_companion_next_owned(&profile,0,1)==0);
    assert(!pk_companion_is_owned(NULL,778));
    assert(pk_companion_next_owned(NULL,0,-1)==0);
    puts("Pokemon owned companion selection tests passed");
    return 0;
}
