#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../knobby/src/pokemon_lab.h"
typedef struct {
    uint32_t roll;
    unsigned index;
    unsigned fail_after;
} sample_t;
static bool rng(void *v,uint32_t *out)
{
    sample_t *s=v;
    if(s->index>=s->fail_after)return false;
    unsigned n=s->index++%3U;
    *out=n==0U?10000U+s->roll:n==1U?0U:4097U;
    return true;
}
int main(void)
{
    pk_lab_t lab={0};
    sample_t sample={0,0,UINT32_MAX};
    assert(!pk_lab_draw(&lab,PK_BALL_MASTER,1,rng,&sample));
    pk_lab_start(&lab);
    assert(lab.active && lab.simulated_level==1U && !lab.master_trainer_badge);
    assert(pk_lab_award_master_badge(&lab));
    assert(lab.master_trainer_badge);
    assert(!pk_lab_set_level(&lab,0));
    assert(pk_lab_set_level(&lab,99) && lab.master_trainer_badge);
    assert(pk_lab_set_level(&lab,100) && lab.master_trainer_badge);
    const unsigned tier_rolls[PK_BALL_COUNT][PK_TIER_COUNT]={
        {0,7500,9500,9950},{0,5500,8500,9800},
        {0,3000,7000,9400},{0,0,0,0}
    };
    for(unsigned ball=0;ball<PK_BALL_COUNT;ball++){
        for(unsigned tier=0;tier<PK_TIER_COUNT;tier++){
            if(!pk_gacha_weight(ball,tier))continue;
            sample_t pick={tier_rolls[ball][tier],0,UINT32_MAX};
            assert(pk_lab_draw(&lab,ball,1,rng,&pick));
            assert(lab.last.tier==tier);
            assert(lab.tiers[ball][tier]>0U);
        }
    }
    sample_t many={0,0,UINT32_MAX};
    assert(pk_lab_draw(&lab,PK_BALL_MASTER,PK_LAB_MAX_BATCH,rng,&many));
    assert(lab.tiers[PK_BALL_MASTER][PK_TIER_LEGENDARY]==1001U);
    assert(lab.draws[PK_BALL_MASTER]==1001U);
    assert(lab.shinies[PK_BALL_MASTER]==0U);
    assert(!pk_lab_draw(&lab,PK_BALL_MASTER,1001,rng,&many));
    assert(!pk_lab_draw(&lab,PK_BALL_COUNT,1,rng,&many));
    assert(!pk_lab_draw(&lab,PK_BALL_POKE,0,rng,&many));
    uint64_t original=lab.draws[PK_BALL_POKE];
    sample_t fail={0,0,4}; /* second draw fails */
    assert(!pk_lab_draw(&lab,PK_BALL_POKE,2,rng,&fail));
    assert(lab.draws[PK_BALL_POKE]==original);
    assert(lab.has_last);
    pk_lab_reset(&lab);
    assert(!lab.active && lab.draws[PK_BALL_MASTER]==0U &&
           !lab.master_trainer_badge && !lab.has_last);
    assert(!pk_lab_award_master_badge(&lab));
    puts("Pokemon isolated unlimited lab tests passed");
    return 0;
}
