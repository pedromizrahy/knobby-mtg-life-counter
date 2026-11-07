#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../knobby/src/pokemon_starter_roll.h"
static bool rng(void *ctx,uint32_t *out){
    uint32_t *seed=(uint32_t*)ctx;
    *seed=(*seed*1664525U)+1013904223U;
    *out=*seed;
    return true;
}
static bool fail(void *ctx,uint32_t *out){(void)ctx;(void)out;return false;}
int main(void){
    const pk_gacha_entry_t catalog[]={
        {1,PK_TIER_COMMON,1},{4,PK_TIER_COMMON,1},
        {25,PK_TIER_RARE,1},{94,PK_TIER_RARE,1},
        {6,PK_TIER_EPIC,1},{9,PK_TIER_EPIC,1},
        {150,PK_TIER_LEGENDARY,1},{151,PK_TIER_LEGENDARY,1}
    };
    assert(PK_STARTER_TOTAL_REVEALS==6U);
    assert(pk_gacha_weight(PK_BALL_POKE,PK_TIER_COMMON)==7500);
    assert(pk_gacha_weight(PK_BALL_POKE,PK_TIER_RARE)==2000);
    assert(pk_gacha_weight(PK_BALL_POKE,PK_TIER_EPIC)==450);
    assert(pk_gacha_weight(PK_BALL_POKE,PK_TIER_LEGENDARY)==50);
    uint32_t seed=12345U;
    pk_starter_state_t s={0};
    assert(pk_starter_begin(&s,catalog,8,rng,&seed));
    assert(s.reveal_count==1 && !s.kept && pk_starter_state_valid(&s));
    uint16_t first=s.current.species;
    assert(!pk_starter_reroll(&s,catalog,8,fail,NULL));
    assert(s.reveal_count==1 && s.current.species==first);
    for(unsigned i=0;i<PK_STARTER_REROLLS;i++){
        assert(pk_starter_reroll(&s,catalog,8,rng,&seed));
        assert(s.reveal_count==i+2U);
        assert(pk_starter_state_valid(&s));
        assert(s.current.species!=first);
    }
    assert(!pk_starter_reroll(&s,catalog,8,rng,&seed));
    pk_gacha_result_t chosen={0};
    assert(pk_starter_keep(&s,&chosen));
    assert(chosen.species==s.current.species && s.kept);
    assert(!pk_starter_keep(&s,&chosen));
    assert(!pk_starter_reroll(&s,catalog,8,rng,&seed));
    pk_starter_state_t early={0};
    assert(pk_starter_begin(&early,catalog,8,rng,&seed));
    assert(pk_starter_keep(&early,&chosen));
    assert(early.reveal_count==1 && !pk_starter_reroll(&early,catalog,8,rng,&seed));
    puts("KEEP-or-REROLL sequential Pokeball starter policy PASS");
    return 0;
}
