#include <assert.h>
#include <stdio.h>
#include "../knobby/src/pokemon_progress.h"
int main(void) {
 pk_profile_t p; pk_reward_t reward;
 pk_profile_init(&p);
 assert(pk_team_add(&p,778,false));
 assert(pk_dex_has(&p,778,false));
 assert(!pk_team_add(&p,0,false));
 assert(pk_award_game(&p,100,false,&reward));
 assert(reward.xp_awarded==50);
 assert(!pk_award_game(&p,100,true,&reward));
 for(unsigned id=101;id<=105;id++) assert(pk_award_game(&p,id,true,&reward));
 assert(p.wins==5 && p.balls[PK_BALL_ULTRA]==1);
 for(unsigned id=1;id<=5;id++) assert(pk_team_add(&p,id,false));
 assert(!pk_team_add(&p,25,false));
 assert(p.team_count==6);
 puts("Pokemon progress tests passed");
 return 0;
}
