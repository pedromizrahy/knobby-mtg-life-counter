#include <assert.h>
#include <stdio.h>
#include "../knobby/src/pokemon_achievements.h"

int main(void)
{
    pk_profile_t before,after;
    pk_reward_t reward;
    pk_profile_init(&before);
    assert(pk_team_add(&before,778,false));
    before.team[0].level=99;
    before.team[0].xp=0;
    after=before;
    assert(pk_award_game(&after,101,true,&reward));
    assert(after.team[0].level==100);
    assert(pk_master_trainer_first_level_100(&before,&after));
    assert(reward.new_masterballs==0);
    assert(after.balls[PK_BALL_MASTER]==0);

    /* Replaying or re-checking a snapshot cannot create another crossing. */
    assert(!pk_master_trainer_first_level_100(&after,&after));
    assert(!pk_master_trainer_first_level_100(&after,&before));
    assert(!pk_award_game(&after,101,true,&reward));

    /* No awards on ordinary level-ups or unconfirmed snapshots. */
    pk_profile_init(&before);
    assert(pk_team_add(&before,778,false));
    after=before;
    assert(pk_award_game(&after,102,false,&reward));
    assert(!pk_master_trainer_first_level_100(&before,&after));
    after=before;after.team[0].level=100;
    assert(!pk_master_trainer_first_level_100(&before,&after));

    /* If someone else already reached 100, a second level 100 is NOT
     * the unique, first-Pokemon achievement. */
    pk_profile_init(&before);
    assert(pk_team_add(&before,778,false));
    assert(pk_team_add(&before,25,false));
    before.team[0].level=100;
    before.team[1].level=99;
    before.active_index=1;
    after=before;
    assert(pk_award_game(&after,103,true,&reward));
    assert(!pk_master_trainer_first_level_100(&before,&after));

    /* Different player / team composition must not count. */
    pk_profile_init(&before);
    assert(pk_team_add(&before,778,false));
    before.team[0].level=99;
    after=before;
    assert(pk_award_game(&after,104,true,&reward));
    after.team[0].species=25;
    assert(!pk_master_trainer_first_level_100(&before,&after));
    assert(!pk_master_trainer_first_level_100(NULL,&after));
    assert(!pk_master_trainer_first_level_100(&before,NULL));
    puts("Pokemon Master Trainer eligibility tests passed");
    return 0;
}
