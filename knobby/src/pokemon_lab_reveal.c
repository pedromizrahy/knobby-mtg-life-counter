#include "pokemon_lab_reveal.h"
#include <stdio.h>
#include <string.h>
const char *pk_lab_fixture_name(uint16_t species)
{
    switch(species){
    case 25:return "PIKACHU";
    case 778:return "MIMIKYU";
    case 149:return "DRAGONITE";
    case 150:return "MEWTWO";
    default:return NULL;
    }
}
const char *pk_lab_tier_name(uint8_t tier)
{
    static const char *const names[]={"COMMON","RARE","EPIC","LEGENDARY"};
    return tier<PK_TIER_COUNT?names[tier]:NULL;
}
bool pk_lab_reveal_text(const pk_gacha_result_t *result,
                        char *buffer,size_t capacity)
{
    if(!result || !buffer || !capacity || result->shiny>1U)return false;
    const char *name=pk_lab_fixture_name(result->species);
    const char *tier=pk_lab_tier_name(result->tier);
    if(!name || !tier)return false;
    /* Cross-check the synthetic one-species-per-tier lab pool. */
    static const uint8_t tiers[4]={
        PK_TIER_COMMON,PK_TIER_RARE,PK_TIER_EPIC,PK_TIER_LEGENDARY
    };
    static const uint16_t ids[4]={25,778,149,150};
    bool found=false;
    for(unsigned i=0;i<4;i++)
        if(ids[i]==result->species && tiers[i]==result->tier)
            found=true;
    if(!found)return false;
    char tmp[80];
    int n=snprintf(tmp,sizeof(tmp),"%s  #%u\n%s%s  [SIM]",
                   name,(unsigned)result->species,tier,
                   result->shiny?"  *SHINY*":"");
    if(n<0 || (size_t)n>=sizeof(tmp) || (size_t)n>=capacity)return false;
    memcpy(buffer,tmp,(size_t)n+1U);
    return true;
}
