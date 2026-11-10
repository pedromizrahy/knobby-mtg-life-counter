#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_pc.h"
static pk_mon_t mon(unsigned id,unsigned shiny,unsigned lvl,unsigned xp){
    pk_mon_t m={(uint16_t)id,(uint8_t)shiny,(uint8_t)lvl,(uint32_t)xp};
    return m;
}
int main(void){
    pk_profile_t profile;
    pk_profile_init(&profile);
    assert(!pk_pc_set_main(&profile,0));
    assert(pk_team_add(&profile,778,false));
    assert(pk_team_add(&profile,25,false));
    assert(pk_team_add(&profile,6,false));
    assert(pk_pc_set_main(&profile,1));
    assert(profile.active_index==1U && !pk_pc_set_main(&profile,3));
    assert(pk_register_discovery(&profile,150,false));
    pk_mon_t buffer[4]={0};
    pk_pc_t pc={buffer,0,4};
    /* Merely finding a Pokemon in the Dex does not put it in the PC. */
    assert(pk_dex_has(&profile,150,false) && pc.count==0);
    assert(!pk_pc_swap_team(&profile,&pc,0,0));
    assert(pk_pc_add_captured(&pc,mon(150,1,55,3000)));
    assert(pk_pc_add_captured(&pc,mon(25,0,15,700)));
    assert(pk_pc_swap_team(&profile,&pc,0,0));
    assert(profile.team[0].species==150 && profile.team[0].shiny==1);
    assert(profile.team[0].level==55 && profile.team[0].xp==3000);
    assert(pc.entries[0].species==778);
    assert(profile.active_index==1 && profile.team[1].species==25);
    assert(pk_pc_take_team(&profile,&pc,1));
    assert(profile.team_count==4 && profile.team[3].species==25);
    assert(pc.count==1 && !pk_pc_deposit_team(&profile,&pc,1));
    assert(pk_pc_deposit_team(&profile,&pc,0));
    assert(profile.team_count==3 && profile.active_index==0);
    assert(profile.team[0].species==25 && pc.entries[1].species==150);
    assert(!pk_pc_add_captured(&pc,mon(0,0,1,0)));
    assert(!pk_pc_add_captured(&pc,mon(151,2,1,0)));
    assert(!pk_pc_add_captured(&pc,mon(151,0,0,0)));
    assert(!pk_pc_take_team(&profile,&pc,4));
    pk_mon_t before=profile.team[2];
    assert(!pk_pc_swap_team(&profile,&pc,99,0));
    assert(memcmp(&before,&profile.team[2],sizeof(before))==0);
    pk_pc_t invalid={NULL,1,2};
    assert(!pk_pc_add_captured(&invalid,mon(151,0,1,0)));
    puts("Pokemon PC/team/main pure model tests passed");
    return 0;
}
