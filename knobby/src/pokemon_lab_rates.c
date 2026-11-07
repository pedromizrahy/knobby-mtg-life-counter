#include "pokemon_lab_rates.h"
#include <stdio.h>
#include <string.h>
bool pk_lab_rates_format(const pk_lab_t *lab,uint8_t ball,
                         char *out,size_t capacity)
{
    if(!lab || !out || !capacity || !lab->active ||
       ball>=PK_BALL_COUNT)return false;
    char tmp[320]={0};
    const char *const names[PK_TIER_COUNT]={"COMMON","RARE","EPIC","LEGEND"};
    size_t used=0;
    uint64_t total=lab->draws[ball];
    for(unsigned i=0;i<PK_TIER_COUNT;i++){
        const unsigned target=pk_gacha_weight(ball,(uint8_t)i);
        /* Avoid count*10000 overflow even with extremely long USB tests. */
        const unsigned observed=total?
            (unsigned)((lab->tiers[ball][i]/total)*10000ULL+
            ((lab->tiers[ball][i]%total)*10000ULL)/total):0U;
        int n=snprintf(tmp+used,sizeof(tmp)-used,
            "%s %u.%02u%% | %u.%02u%%\n",
            names[i],target/100U,target%100U,
            observed/100U,observed%100U);
        if(n<0 || (size_t)n>=sizeof(tmp)-used)return false;
        used+=(size_t)n;
    }
    int n=snprintf(tmp+used,sizeof(tmp)-used,
        "\nTOTAL: %llu draws\nSHINY: %llu (target 1/4096)",
        (unsigned long long)total,
        (unsigned long long)lab->shinies[ball]);
    if(n<0 || (size_t)n>=sizeof(tmp)-used)return false;
    used+=(size_t)n;
    if(used+1>capacity)return false;
    memcpy(out,tmp,used+1);
    return true;
}
