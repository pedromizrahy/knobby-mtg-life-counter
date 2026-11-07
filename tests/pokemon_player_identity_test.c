#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../knobby/src/pokemon_player_identity.h"
int main(void)
{
    assert(!pk_player_id_valid(0));
    assert(pk_player_id_valid(1));
    assert(pk_player_id_valid(184199));
    assert(pk_player_id_valid(0xFFFFFFFBULL));
    assert(!pk_player_id_valid(0xFFFFFFFCULL));
    assert(!pk_player_id_valid(0xFFFFFFFDULL));
    assert(!pk_player_id_valid(0xFFFFFFFEULL));
    assert(!pk_player_id_valid(0xFFFFFFFFULL));
    assert(!pk_player_id_valid(0x100000000ULL));
    uint64_t members[]={184199,184200,184201};
    assert(pk_player_id_in_roster(184200,members,3));
    assert(!pk_player_id_in_roster(184999,members,3));
    assert(!pk_player_id_in_roster(184200,NULL,3));
    assert(!pk_player_id_in_roster(184200,members,1));
    members[2]=184199;
    assert(!pk_player_id_in_roster(184200,members,3));
    members[2]=0;
    assert(!pk_player_id_in_roster(184200,members,3));
    members[2]=0xFFFFFFFDULL;
    assert(!pk_player_id_in_roster(184200,members,3));
    puts("Pokemon Playgroup player identity tests passed");
    return 0;
}
