#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_lab_rates.h"
int main(void)
{
    pk_lab_t state={0};
    char text[320]="UNCHANGED";
    assert(!pk_lab_rates_format(&state,PK_BALL_POKE,text,sizeof(text)));
    assert(!strcmp(text,"UNCHANGED"));
    pk_lab_start(&state);
    assert(pk_lab_rates_format(&state,PK_BALL_POKE,text,sizeof(text)));
    assert(strstr(text,"COMMON 75.00% | 0.00%\n"));
    assert(strstr(text,"TOTAL: 0 draws"));
    state.draws[PK_BALL_POKE]=1000;
    state.tiers[PK_BALL_POKE][PK_TIER_COMMON]=750;
    state.tiers[PK_BALL_POKE][PK_TIER_RARE]=200;
    state.tiers[PK_BALL_POKE][PK_TIER_EPIC]=45;
    state.tiers[PK_BALL_POKE][PK_TIER_LEGENDARY]=5;
    state.shinies[PK_BALL_POKE]=1;
    assert(pk_lab_rates_format(&state,PK_BALL_POKE,text,sizeof(text)));
    assert(strstr(text,"COMMON 75.00% | 75.00%\n"));
    assert(strstr(text,"RARE 20.00% | 20.00%\n"));
    assert(strstr(text,"EPIC 4.50% | 4.50%\n"));
    assert(strstr(text,"LEGEND 0.50% | 0.50%\n"));
    assert(strstr(text,"SHINY: 1 (target 1/4096)"));
    char small[5]="KEEP";
    assert(!pk_lab_rates_format(&state,PK_BALL_POKE,small,sizeof(small)));
    assert(!strcmp(small,"KEEP"));
    assert(!pk_lab_rates_format(&state,PK_BALL_COUNT,text,sizeof(text)));
    state.draws[PK_BALL_MASTER]=UINT64_MAX;
    state.tiers[PK_BALL_MASTER][PK_TIER_LEGENDARY]=UINT64_MAX;
    assert(pk_lab_rates_format(&state,PK_BALL_MASTER,text,sizeof(text)));
    assert(strstr(text,"LEGEND 100.00% | 100.00%\n"));
    puts("Pokemon LAB rate formatting tests passed");
    return 0;
}
