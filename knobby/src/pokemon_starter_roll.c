#include "pokemon_starter_roll.h"
#include <string.h>
/* A player sees one result at a time. Discarded reveals cannot be reclaimed.
 * The caller must durably persist state and current before displaying them.
 * This pure engine does NOT write storage or grant a Pokemon. */
static bool draw_new(pk_starter_state_t *state,
                     const pk_gacha_entry_t *catalog,size_t count,
                     pk_gacha_rng_t rng,void *ctx)
{
    if(!state || !rng ||
       !pk_gacha_catalog_ready(PK_BALL_POKE,catalog,count) ||
       count<PK_STARTER_TOTAL_REVEALS)return false;
    pk_gacha_result_t result={0};
    for(unsigned attempt=0;attempt<1024U;attempt++){
        if(!pk_gacha_preview_prevalidated(PK_BALL_POKE,catalog,count,
                                           rng,ctx,&result))return false;
        bool seen=false;
        for(unsigned i=0;i<state->reveal_count;i++)
            if(state->revealed_species[i]==result.species)seen=true;
        if(!seen){
            state->current=result;
            state->revealed_species[state->reveal_count]=result.species;
            state->reveal_count++;
            return true;
        }
    }
    return false;
}
bool pk_starter_begin(pk_starter_state_t *state,
                      const pk_gacha_entry_t *catalog,size_t count,
                      pk_gacha_rng_t rng,void *ctx)
{
    if(!state)return false;
    pk_starter_state_t fresh={0};
    if(!draw_new(&fresh,catalog,count,rng,ctx))return false;
    *state=fresh;
    return true;
}
bool pk_starter_reroll(pk_starter_state_t *state,
                      const pk_gacha_entry_t *catalog,size_t count,
                      pk_gacha_rng_t rng,void *ctx)
{
    if(!state || state->kept || state->reveal_count<1U ||
       state->reveal_count>=PK_STARTER_TOTAL_REVEALS ||
       !pk_starter_state_valid(state))return false;
    pk_starter_state_t next=*state;
    if(!draw_new(&next,catalog,count,rng,ctx))return false;
    *state=next; /* Discard previous current only after successful new draw. */
    return true;
}
bool pk_starter_keep(pk_starter_state_t *state,
                     pk_gacha_result_t *chosen)
{
    if(!state || !chosen || state->kept ||
       !pk_starter_state_valid(state))return false;
    *chosen=state->current;
    state->kept=true; /* Persist this choice atomically before creation. */
    return true;
}
bool pk_starter_state_valid(const pk_starter_state_t *state)
{
    if(!state || !state->reveal_count ||
       state->reveal_count>PK_STARTER_TOTAL_REVEALS ||
       state->current.species<1U ||
       state->current.species>PK_DEX_SPECIES ||
       state->current.tier>=PK_TIER_COUNT ||
       state->current.shiny>1U ||
       state->revealed_species[state->reveal_count-1U]!=state->current.species)
        return false;
    for(unsigned i=0;i<state->reveal_count;i++){
        if(!state->revealed_species[i] ||
           state->revealed_species[i]>PK_DEX_SPECIES)return false;
        for(unsigned j=0;j<i;j++)
            if(state->revealed_species[i]==state->revealed_species[j])return false;
    }
    return true;
}
