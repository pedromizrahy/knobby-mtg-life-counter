#include "pokemon_gacha.h"
#include <string.h>
/* Proposed DIAL odds, NOT canonical Pokémon capture rates.
 * Poké: 75 / 20 / 4.5 / 0.5
 * Great: 55 / 30 / 13 / 2
 * Ultra: 30 / 40 / 24 / 6
 * Master: guaranteed legendary
 */
static const uint16_t kWeights[PK_BALL_COUNT][PK_TIER_COUNT]={
    {7500,2000,450,50},
    {5500,3000,1300,200},
    {3000,4000,2400,600},
    {0,0,0,10000}
};
uint16_t pk_gacha_weight(uint8_t ball,uint8_t tier)
{
    if(ball>=PK_BALL_COUNT || tier>=PK_TIER_COUNT)return 0;
    return kWeights[ball][tier];
}
/* Rejection sampling avoids modulo bias; entropy MUST come from a
 * trustworthy source provided by the caller, not timestamps or rand().
 */
static bool bounded(pk_gacha_rng_t rng,void *ctx,uint32_t upper,uint32_t *out)
{
    if(!rng || !out || !upper)return false;
    const uint32_t threshold=(uint32_t)(-upper)%upper;
    for(unsigned tries=0;tries<32;tries++){
        uint32_t value=0;
        if(!rng(ctx,&value))return false;
        if(value>=threshold){*out=value%upper;return true;}
    }
    return false;
}
bool pk_gacha_catalog_ready(uint8_t ball,
                            const pk_gacha_entry_t *catalog,size_t count)
{
    if(ball>=PK_BALL_COUNT || !catalog || count==0 ||
       count>PK_DEX_SPECIES)return false;
    uint8_t available[PK_TIER_COUNT]={0};
    for(size_t i=0;i<count;i++){
        const pk_gacha_entry_t *e=&catalog[i];
        if(e->species==0 || e->species>PK_DEX_SPECIES ||
           e->tier>=PK_TIER_COUNT || e->sprite_ready!=1)
            return false;
        for(size_t j=0;j<i;j++)
            if(catalog[j].species==e->species)return false;
        available[e->tier]=1;
    }
    for(uint8_t t=0;t<PK_TIER_COUNT;t++)
        if(kWeights[ball][t] && !available[t])return false;
    return true;
}
bool pk_gacha_preview(uint8_t ball,
                      const pk_gacha_entry_t *catalog,size_t count,
                      pk_gacha_rng_t rng,void *ctx,
                      pk_gacha_result_t *out)
{
    if(!rng || !out || !pk_gacha_catalog_ready(ball,catalog,count))
        return false;
    uint32_t roll=0;
    if(!bounded(rng,ctx,10000U,&roll))return false;
    uint8_t tier=PK_TIER_COUNT;
    uint32_t cumulative=0;
    for(uint8_t t=0;t<PK_TIER_COUNT;t++){
        cumulative+=kWeights[ball][t];
        if(roll<cumulative){tier=t;break;}
    }
    if(tier==PK_TIER_COUNT)return false;
    uint32_t candidates=0;
    for(size_t i=0;i<count;i++)
        if(catalog[i].tier==tier)candidates++;
    /* Strict failure: no reinterpretation of rarity when a tier is empty. */
    if(candidates==0)return false;
    uint32_t pick=0,shiny=0;
    if(!bounded(rng,ctx,candidates,&pick) ||
       !bounded(rng,ctx,PK_GACHA_SHINY_DENOMINATOR,&shiny))
        return false;
    for(size_t i=0;i<count;i++){
        if(catalog[i].tier!=tier)continue;
        if(pick--==0U){
            const pk_gacha_result_t selected={
                catalog[i].species,tier,(uint8_t)(shiny==0U)
            };
            *out=selected;
            return true;
        }
    }
    return false;
}
