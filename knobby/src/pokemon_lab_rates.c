#include "pokemon_lab_rates.h"
#include <stdio.h>
#include <string.h>
/* Exact floor(count * 10000 / total), without overflowing uint64_t when
 * a LAB has accumulated very large counters. The quotient never exceeds
 * 10000, so 14 binary digits suffice for the multiplier. */
static unsigned basis_points(uint64_t count,uint64_t total)
{
    if(!total)return 0U;
    if(count>=total)return 10000U;
    uint64_t remainder=0;
    unsigned quotient=0;
    for(int bit=13;bit>=0;--bit){
        quotient*=2U;
        if(remainder>=total-remainder){
            remainder-=total-remainder;
            quotient++;
        }else remainder+=remainder;
        if((10000U & (1U<<bit))!=0U){
            if(remainder>=total-count){
                remainder-=total-count;
                quotient++;
            }else remainder+=count;
        }
    }
    return quotient;
}
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
        const unsigned observed=basis_points(lab->tiers[ball][i],total);
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
