#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_lab_reveal.h"
int main(void)
{
    const uint16_t ids[]={25,778,149,150};
    for(unsigned i=0;i<4;i++){
        pk_gacha_result_t item={ids[i],(uint8_t)i,0};
        char label[80]={0};
        assert(pk_lab_reveal_text(&item,label,sizeof(label)));
        assert(strstr(label,"[SIM]") && strstr(label,pk_lab_tier_name((uint8_t)i)));
        item.shiny=1;
        assert(pk_lab_reveal_text(&item,label,sizeof(label)));
        assert(strstr(label,"*SHINY*"));
    }
    pk_gacha_result_t invalid={778,PK_TIER_LEGENDARY,0};
    char unchanged[80]="UNCHANGED";
    assert(!pk_lab_reveal_text(&invalid,unchanged,sizeof(unchanged)));
    assert(!strcmp(unchanged,"UNCHANGED"));
    invalid.species=999;
    assert(!pk_lab_reveal_text(&invalid,unchanged,sizeof(unchanged)));
    invalid.species=778;invalid.tier=PK_TIER_RARE;invalid.shiny=2;
    assert(!pk_lab_reveal_text(&invalid,unchanged,sizeof(unchanged)));
    invalid.shiny=0;
    assert(!pk_lab_reveal_text(&invalid,unchanged,5));
    assert(!pk_lab_reveal_text(NULL,unchanged,sizeof(unchanged)));
    assert(!pk_lab_reveal_text(&invalid,NULL,sizeof(unchanged)));
    assert(!strcmp(unchanged,"UNCHANGED"));
    puts("Pokemon LAB reveal formatting tests passed");
    return 0;
}
