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
    /* Two per tier provides full coverage; the regular ball weights are
     * tested separately by the canonical pokemon_gacha_test. */
    const pk_gacha_entry_t catalog[]={
        {1,PK_TIER_COMMON,1},{4,PK_TIER_COMMON,1},
        {25,PK_TIER_RARE,1},{94,PK_TIER_RARE,1},
        {6,PK_TIER_EPIC,1},{9,PK_TIER_EPIC,1},
        {150,PK_TIER_LEGENDARY,1},{151,PK_TIER_LEGENDARY,1}
    };
    uint32_t seed=12345U;
    pk_gacha_result_t out[PK_STARTER_OPTIONS]={0};
    assert(PK_STARTER_OPTIONS==PK_STARTER_REROLLS+1U);
    assert(pk_gacha_weight(PK_BALL_POKE,PK_TIER_COMMON)==7500);
    assert(pk_gacha_weight(PK_BALL_POKE,PK_TIER_RARE)==2000);
    assert(pk_gacha_weight(PK_BALL_POKE,PK_TIER_EPIC)==450);
    assert(pk_gacha_weight(PK_BALL_POKE,PK_TIER_LEGENDARY)==50);
    assert(pk_starter_roll_six(catalog,8,rng,&seed,out));
    assert(pk_starter_choice_valid(out,PK_STARTER_OPTIONS,0));
    assert(pk_starter_choice_valid(out,PK_STARTER_OPTIONS,5));
    assert(!pk_starter_choice_valid(out,PK_STARTER_OPTIONS,6));
    assert(!pk_starter_choice_valid(out,0,0));
    pk_gacha_result_t previous[PK_STARTER_OPTIONS];
    for(unsigned i=0;i<PK_STARTER_OPTIONS;i++)previous[i]=out[i];
    assert(!pk_starter_roll_six(catalog,8,fail,NULL,out));
    for(unsigned i=0;i<PK_STARTER_OPTIONS;i++)
        assert(previous[i].species==out[i].species);
    printf("Starter Pokeball-level five rerolls policy PASS\n");
    return 0;
}
