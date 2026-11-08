#include "pokemon_evolution.h"
#include <string.h>
/* Audited Gen I/II subset of the public PokeAPI CSV species dependencies.
 * Future pre-evolutions are excluded from initial-form gacha, even if the
 * current SD collection has only the first 251 Pokemon.
 * Rarity policy lives in pokemon_lab_dex; evolution is independent. */
static const uint16_t kBase[]={
    1U,4U,7U,10U,13U,16U,19U,21U,23U,27U,29U,32U,37U,41U,43U,46U,
    48U,50U,52U,54U,56U,58U,60U,63U,66U,69U,72U,74U,77U,79U,81U,83U,
    84U,86U,88U,90U,92U,95U,96U,98U,100U,102U,104U,108U,109U,111U,114U,115U,
    116U,118U,120U,123U,127U,128U,129U,131U,132U,133U,137U,138U,140U,142U,144U,145U,
    146U,147U,150U,151U,152U,155U,158U,161U,163U,165U,167U,170U,172U,173U,174U,175U,
    177U,179U,187U,190U,191U,193U,194U,198U,200U,201U,203U,204U,206U,207U,209U,211U,
    213U,214U,215U,216U,218U,220U,222U,223U,225U,227U,228U,231U,234U,235U,236U,238U,
    239U,240U,241U,243U,244U,245U,246U,249U,250U,251U,
};
static const pk_evo_rule_t kRules[]={
    {1U,2U,1U,16U,0U},
    {2U,3U,1U,32U,0U},
    {4U,5U,1U,16U,0U},
    {5U,6U,1U,36U,0U},
    {7U,8U,1U,16U,0U},
    {8U,9U,1U,36U,0U},
    {10U,11U,1U,7U,0U},
    {11U,12U,1U,10U,0U},
    {13U,14U,1U,7U,0U},
    {14U,15U,1U,10U,0U},
    {16U,17U,1U,18U,0U},
    {17U,18U,1U,36U,0U},
    {19U,20U,1U,20U,0U},
    {19U,20U,5U,0U,0U},
    {21U,22U,1U,20U,0U},
    {23U,24U,1U,22U,0U},
    {25U,26U,2U,0U,83U},
    {27U,28U,1U,22U,0U},
    {27U,28U,2U,0U,885U},
    {29U,30U,1U,16U,0U},
    {30U,31U,2U,0U,81U},
    {32U,33U,1U,16U,0U},
    {33U,34U,2U,0U,81U},
    {35U,36U,2U,0U,81U},
    {37U,38U,2U,0U,82U},
    {39U,40U,2U,0U,81U},
    {41U,42U,1U,22U,0U},
    {42U,169U,4U,0U,0U},
    {43U,44U,1U,21U,0U},
    {44U,45U,2U,0U,85U},
    {44U,182U,2U,0U,80U},
    {46U,47U,1U,24U,0U},
    {48U,49U,1U,31U,0U},
    {50U,51U,1U,26U,0U},
    {50U,51U,5U,0U,0U},
    {52U,53U,1U,28U,0U},
    {52U,53U,4U,0U,0U},
    {54U,55U,1U,33U,0U},
    {56U,57U,1U,28U,0U},
    {58U,59U,2U,0U,82U},
    {60U,61U,1U,25U,0U},
    {61U,62U,2U,0U,84U},
    {61U,186U,3U,0U,198U},
    {63U,64U,1U,16U,0U},
    {64U,65U,3U,0U,0U},
    {66U,67U,1U,28U,0U},
    {67U,68U,3U,0U,0U},
    {69U,70U,1U,21U,0U},
    {70U,71U,2U,0U,85U},
    {72U,73U,1U,30U,0U},
    {74U,75U,1U,25U,0U},
    {74U,75U,5U,0U,0U},
    {75U,76U,3U,0U,0U},
    {77U,78U,1U,40U,0U},
    {77U,78U,5U,0U,0U},
    {79U,80U,1U,37U,0U},
    {79U,80U,2U,0U,1633U},
    {79U,199U,3U,0U,198U},
    {79U,199U,2U,0U,1643U},
    {81U,82U,1U,30U,0U},
    {84U,85U,1U,31U,0U},
    {86U,87U,1U,34U,0U},
    {88U,89U,1U,38U,0U},
    {88U,89U,5U,0U,0U},
    {90U,91U,2U,0U,84U},
    {92U,93U,1U,25U,0U},
    {93U,94U,3U,0U,0U},
    {95U,208U,3U,0U,210U},
    {96U,97U,1U,26U,0U},
    {98U,99U,1U,28U,0U},
    {100U,101U,1U,30U,0U},
    {100U,101U,2U,0U,85U},
    {102U,103U,2U,0U,85U},
    {104U,105U,1U,28U,0U},
    {104U,105U,5U,0U,0U},
    {109U,110U,1U,35U,0U},
    {109U,110U,5U,0U,0U},
    {111U,112U,1U,42U,0U},
    {113U,242U,4U,0U,0U},
    {116U,117U,1U,32U,0U},
    {117U,230U,3U,0U,212U},
    {118U,119U,1U,33U,0U},
    {120U,121U,2U,0U,84U},
    {123U,212U,3U,0U,210U},
    {129U,130U,1U,20U,0U},
    {133U,134U,2U,0U,84U},
    {133U,135U,2U,0U,83U},
    {133U,136U,2U,0U,82U},
    {133U,196U,4U,0U,0U},
    {133U,197U,4U,0U,0U},
    {137U,233U,3U,0U,229U},
    {138U,139U,1U,40U,0U},
    {140U,141U,1U,40U,0U},
    {147U,148U,1U,30U,0U},
    {148U,149U,1U,55U,0U},
    {152U,153U,1U,16U,0U},
    {153U,154U,1U,32U,0U},
    {155U,156U,1U,14U,0U},
    {155U,156U,5U,0U,0U},
    {156U,157U,1U,36U,0U},
    {156U,157U,5U,0U,0U},
    {158U,159U,1U,18U,0U},
    {159U,160U,1U,30U,0U},
    {161U,162U,1U,15U,0U},
    {163U,164U,1U,20U,0U},
    {165U,166U,1U,18U,0U},
    {167U,168U,1U,22U,0U},
    {170U,171U,1U,27U,0U},
    {172U,25U,4U,0U,0U},
    {173U,35U,4U,0U,0U},
    {174U,39U,4U,0U,0U},
    {175U,176U,4U,0U,0U},
    {177U,178U,1U,25U,0U},
    {179U,180U,1U,15U,0U},
    {180U,181U,1U,30U,0U},
    {183U,184U,1U,18U,0U},
    {187U,188U,1U,18U,0U},
    {188U,189U,1U,27U,0U},
    {191U,192U,2U,0U,80U},
    {194U,195U,1U,20U,0U},
    {204U,205U,1U,31U,0U},
    {209U,210U,1U,23U,0U},
    {216U,217U,1U,30U,0U},
    {218U,219U,1U,38U,0U},
    {220U,221U,1U,33U,0U},
    {223U,224U,1U,25U,0U},
    {228U,229U,1U,24U,0U},
    {231U,232U,1U,25U,0U},
    {236U,106U,5U,0U,0U},
    {236U,107U,5U,0U,0U},
    {236U,237U,5U,0U,0U},
    {238U,124U,1U,30U,0U},
    {239U,125U,1U,30U,0U},
    {240U,126U,1U,30U,0U},
    {246U,247U,1U,30U,0U},
    {247U,248U,1U,55U,0U},
};
bool pk_evo_is_base(uint16_t species){
    for(size_t i=0;i<sizeof(kBase)/sizeof(kBase[0]);i++)
        if(kBase[i]==species)return true;
    return false;
}
size_t pk_evo_options(uint16_t from,pk_evo_rule_t *out,size_t cap){
    size_t count=0;
    for(size_t i=0;i<sizeof(kRules)/sizeof(kRules[0]);i++)
        if(kRules[i].from==from)count++;
    if(!out || cap<count)return count;
    size_t next=0;
    for(size_t i=0;i<sizeof(kRules)/sizeof(kRules[0]);i++)
        if(kRules[i].from==from)out[next++]=kRules[i];
    return count;
}
bool pk_evo_level_eligible(uint16_t from,uint16_t to,uint8_t level){
    if(level<1U||level>100U)return false;
    for(size_t i=0;i<sizeof(kRules)/sizeof(kRules[0]);i++)
        if(kRules[i].from==from && kRules[i].to==to &&
           kRules[i].kind==PK_EVO_LEVEL &&
           kRules[i].level>0U && level>=kRules[i].level)
            return true;
    return false;
}
bool pk_evo_prepare_level(const pk_profile_t *original,uint8_t slot,
                          uint16_t to,pk_profile_t *modified){
    if(!original || !modified || original==modified ||
       original->version!=1U || original->team_count>PK_TEAM_MAX ||
       slot>=original->team_count || to==0 || to>PK_DEX_SPECIES)
        return false;
    const pk_mon_t *current=&original->team[slot];
    if(!pk_evo_level_eligible(current->species,to,current->level))return false;
    pk_profile_t draft=*original;
    draft.team[slot].species=to;
    (void)pk_register_discovery(&draft,to,current->shiny!=0U);
    *modified=draft;
    return true;
}
