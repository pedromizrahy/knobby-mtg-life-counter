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
 assert(reward.xp_awarded==300);
 assert(!pk_award_game(&p,100,true,&reward));
 for(unsigned id=101;id<=105;id++) assert(pk_award_game(&p,id,true,&reward));
 assert(p.wins==5 && p.balls[PK_BALL_ULTRA]==1);
 for(unsigned id=1;id<=5;id++) assert(pk_team_add(&p,id,false));
 assert(!pk_team_add(&p,25,false));
 assert(p.team_count==6);
 /* Independent records: same game can reward another player once. */
 pk_profile_t other;
 pk_profile_init(&other);
 assert(pk_team_add(&other,778,false));
 assert(pk_award_game(&other,100,true,&reward));
 assert(!pk_award_game(&other,100,true,&reward));
 assert(other.wins==1 && p.wins==5);
 /* Invalid species never enter the collection or Pokédex. */
 assert(!pk_register_discovery(&p,1026,false));
 assert(!pk_register_discovery(&p,0,false));
 assert(pk_dex_count(&other)==1);
 /* Complete 10 wins to validate second milestone, no double payout. */
 for(unsigned id=106;id<=110;id++) assert(pk_award_game(&p,id,true,&reward));
 assert(p.wins==10 && p.balls[PK_BALL_ULTRA]==2);
 assert(!pk_award_game(&p,110,true,&reward));
 /* One validated win gives 800 XP; level 99 -> 100, without
  * granting an untracked Master Ball on every Pokemon. */
 pk_profile_t milestone;
 pk_profile_init(&milestone);
 assert(pk_team_add(&milestone,778,false));
 milestone.team[0].level=99;
 assert(pk_award_game(&milestone,9001,true,&reward));
 assert(reward.xp_awarded==800);
 assert(milestone.team[0].level==100);
 assert(milestone.balls[PK_BALL_MASTER]==0);
 assert(reward.new_masterballs==0);
 assert(pk_team_add(&milestone,25,false));
 milestone.active_index=1;
 milestone.team[1].level=99;
 assert(pk_award_game(&milestone,9002,true,&reward));
 assert(milestone.team[1].level==100);
 assert(milestone.balls[PK_BALL_MASTER]==0);
 assert(reward.new_masterballs==0);
 assert(!pk_award_game(&milestone,9002,true,&reward));
 puts("Pokemon progress tests passed");
 return 0;
}
