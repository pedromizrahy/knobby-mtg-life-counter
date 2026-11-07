#include "pokemon_starter_roll.h"
#include <string.h>
/* Weight is fixed to the regular Pokeball on EVERY attempt, not upgraded
 * with rerolls. Rejection of already-revealed species is bounded.
 * A complete six-result batch either succeeds or leaves out untouched. */
bool pk_starter_roll_six(const pk_gacha_entry_t *catalog,size_t count,
                         pk_gacha_rng_t rng,void *ctx,
                         pk_gacha_result_t out[PK_STARTER_OPTIONS])
{
    if(!out || !rng || !pk_gacha_catalog_ready(PK_BALL_POKE,catalog,count) ||
       count<PK_STARTER_OPTIONS)return false;
    pk_gacha_result_t picks[PK_STARTER_OPTIONS]={0};
    for(unsigned i=0;i<PK_STARTER_OPTIONS;i++){
        bool picked=false;
        for(unsigned attempt=0;attempt<1024;attempt++){
            pk_gacha_result_t candidate={0};
            if(!pk_gacha_preview_prevalidated(PK_BALL_POKE,catalog,count,
                                                rng,ctx,&candidate))
                return false;
            bool repeat=false;
            for(unsigned j=0;j<i;j++)
                if(candidate.species==picks[j].species){repeat=true;break;}
            if(!repeat){picks[i]=candidate;picked=true;break;}
        }
        if(!picked)return false;
    }
    memcpy(out,picks,sizeof(picks));
    return true;
}
bool pk_starter_choice_valid(const pk_gacha_result_t picks[PK_STARTER_OPTIONS],
                             unsigned count,unsigned choice)
{
    if(!picks || count<1U || count>PK_STARTER_OPTIONS || choice>=count)
        return false;
    for(unsigned i=0;i<count;i++){
        if(picks[i].species<1U || picks[i].species>PK_DEX_SPECIES ||
           picks[i].tier>=PK_TIER_COUNT || picks[i].shiny>1U)
            return false;
        for(unsigned j=0;j<i;j++)
            if(picks[j].species==picks[i].species)return false;
    }
    return true;
}
