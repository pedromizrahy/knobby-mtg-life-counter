#include "pokemon_lab.h"
#include <string.h>
#include <stdint.h>

/* Explicitly synthetic test pool: one sample ID per tier. Do not use
 * these results as a physical sprite-ready collection or real inventory.
 * Actual gacha remains gated on installed art and atomic storage.
 */
static const pk_gacha_entry_t kSynthetic[PK_TIER_COUNT]={
    {25,PK_TIER_COMMON,1},
    {778,PK_TIER_RARE,1},
    {149,PK_TIER_EPIC,1},
    {150,PK_TIER_LEGENDARY,1}
};
void pk_lab_start(pk_lab_t *lab)
{
    if(!lab)return;
    memset(lab,0,sizeof(*lab));
    lab->active=true;
    lab->simulated_level=1U;
}
void pk_lab_reset(pk_lab_t *lab)
{
    if(lab)memset(lab,0,sizeof(*lab));
}
bool pk_lab_award_master_badge(pk_lab_t *lab)
{
    if(!lab || !lab->active)return false;
    lab->master_trainer_badge=true;
    return true;
}
bool pk_lab_set_level(pk_lab_t *lab,uint8_t level)
{
    if(!lab || !lab->active || level<1U || level>100U)return false;
    lab->simulated_level=level;
    if(level==100U)lab->master_trainer_badge=true;
    return true;
}
bool pk_lab_draw(pk_lab_t *lab,uint8_t ball,unsigned count,
                 pk_gacha_rng_t rng,void *rng_ctx)
{
    if(!lab || !lab->active || ball>=PK_BALL_COUNT ||
       count==0U || count>PK_LAB_MAX_BATCH || !rng)
        return false;
    if(lab->draws[ball]>UINT64_MAX-(uint64_t)count)return false;
    /* Commit stats only after the whole batch succeeds. Errors never
     * decrement currency or leave a partially recorded batch. */
    uint64_t tier_counts[PK_TIER_COUNT]={0};
    uint64_t shiny_count=0;
    pk_gacha_result_t last={0};
    for(unsigned i=0;i<count;i++){
        pk_gacha_result_t result={0};
        if(!pk_gacha_preview(ball,kSynthetic,PK_TIER_COUNT,
                             rng,rng_ctx,&result))
            return false;
        tier_counts[result.tier]++;
        if(result.shiny)shiny_count++;
        last=result;
    }
    for(unsigned t=0;t<PK_TIER_COUNT;t++){
        if(lab->tiers[ball][t]>UINT64_MAX-tier_counts[t])return false;
    }
    if(lab->shinies[ball]>UINT64_MAX-shiny_count)return false;
    for(unsigned t=0;t<PK_TIER_COUNT;t++)
        lab->tiers[ball][t]+=tier_counts[t];
    lab->draws[ball]+=count;
    lab->shinies[ball]+=shiny_count;
    lab->last=last;
    lab->last_ball=ball;
    lab->has_last=true;
    return true;
}
