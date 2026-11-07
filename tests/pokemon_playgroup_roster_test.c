#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_playgroup_roster.h"
static bool read_profile(void *ctx,uint32_t id,pk_profile_t *p) {
    unsigned *calls=(unsigned *)ctx; ++*calls;
    if(id==3U)return false; /* missing profile */
    pk_profile_init(p);
    assert(pk_team_add(p,id==1U?778U:25U,false));
    if(id==2U){
        assert(pk_team_add(p,448U,true));
        p->active_index=1U;
        p->team[1].level=23U;
        p->team[1].xp=567U;
    }
    return true;
}
int main(void){
    unsigned calls=0;
    pk_pg_seat_t seats[6]={
        {1U,"P1"},{2U,"P2"},{3U,"P3"},{0U,"GUEST"},
        {2U,"CLONE"},{0xFFFFFFFCULL,"RESERVED"}
    };
    pk_pg_main_t views[6];
    assert(pk_pg_collect_main(seats,6,read_profile,&calls,views,6));
    assert(calls==2U);
    assert(views[0].has_main && views[0].species==778U);
    /* duplicate Playgroup user IDs are NOT accepted for either seat */
    assert(!views[1].has_main && !views[4].has_main);
    assert(!views[3].has_main && !views[5].has_main);
    assert(!views[2].has_main);
    assert(strcmp(views[4].name,"CLONE")==0);
    seats[4].user_id=4U;
    calls=0;
    assert(pk_pg_collect_main(seats,6,read_profile,&calls,views,6));
    assert(calls==4U);
    assert(views[1].has_main && views[1].species==448U);
    assert(views[1].shiny==1U && views[1].level==23U);
    assert(views[1].xp==567U && !views[2].has_main);
    /* Malformed/unterminated names reject without overwriting old view. */
    memset(seats[0].name,'A',sizeof(seats[0].name));
    pk_pg_main_t old=views[0];
    assert(!pk_pg_collect_main(seats,6,read_profile,&calls,views,6));
    assert(memcmp(&old,&views[0],sizeof(old))==0);
    assert(!pk_pg_collect_main(seats,6,read_profile,&calls,views,5));
    puts("Pokemon Playgroup main-only roster tests passed");
    return 0;
}
