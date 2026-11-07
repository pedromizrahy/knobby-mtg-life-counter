#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_lab.h"
#include "../knobby/src/pokemon_lab_dex.h"
static unsigned nth=0;
static bool rng(void *ctx,uint32_t *out)
{
    (void)ctx;
    *out=(uint32_t)(nth++*2654435761U);
    return true;
}
int main(void)
{
    size_t size=0;
    const pk_gacha_entry_t *entries=pk_lab_dex_catalog(&size);
    assert(entries && size==251U);
    unsigned tier_count[PK_TIER_COUNT]={0};
    for(unsigned id=1;id<=251;id++){
        assert(entries[id-1].species==id);
        assert(pk_lab_dex_name((uint16_t)id));
        assert(strlen(pk_lab_dex_name((uint16_t)id))>0);
        assert(entries[id-1].tier==pk_lab_dex_tier((uint16_t)id));
        assert(entries[id-1].sprite_ready==1);
        assert(entries[id-1].tier<PK_TIER_COUNT);
        tier_count[entries[id-1].tier]++;
    }
    assert(tier_count[0]==114 && tier_count[1]==103 &&
           tier_count[2]==23 && tier_count[3]==11);
    assert(!strcmp(pk_lab_dex_name(1),"Bulbasaur"));
    assert(!strcmp(pk_lab_dex_name(151),"Mew"));
    assert(!strcmp(pk_lab_dex_name(251),"Celebi"));
    assert(!strcmp(pk_lab_dex_name(778),"MIMIKYU"));
    assert(pk_lab_dex_name(252)==NULL);
    assert(pk_lab_dex_tier(252)==PK_TIER_COUNT);
    for(unsigned ball=0;ball<PK_BALL_COUNT;ball++)
        assert(pk_gacha_catalog_ready(ball,entries,size));
    pk_lab_t lab={0};
    pk_lab_start(&lab);
    for(unsigned ball=0;ball<PK_BALL_COUNT;ball++){
        assert(pk_lab_draw(&lab,ball,1000U,rng,NULL));
        assert(lab.draws[ball]==1000U);
        uint64_t counted=0;
        for(unsigned tier=0;tier<PK_TIER_COUNT;tier++)
            counted+=lab.tiers[ball][tier];
        assert(counted==1000U);
    }
    assert(lab.tiers[PK_BALL_MASTER][PK_TIER_LEGENDARY]==1000U);
    puts("Kanto+Johto 251-species LAB simulation and rates passed");
    return 0;
}
