#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_evolution.h"
int main(void){
    assert(pk_evo_is_base(161)); /* Sentret */
    assert(!pk_evo_is_base(162));/* Furret */
    assert(pk_evo_is_base(133)); /* Eevee */
    assert(!pk_evo_is_base(134));
    assert(!pk_evo_is_base(196));
    assert(pk_evo_is_base(172)); /* Pichu */
    assert(!pk_evo_is_base(25)); /* Pikachu */
    assert(pk_evo_is_base(150)); /* Mewtwo, no baby */
    assert(!pk_evo_is_base(143)); /* Snorlax has future baby Munchlax */
    assert(!pk_evo_is_base(0));
    pk_evo_rule_t opts[12]={{0}};
    size_t n=pk_evo_options(133,opts,12);
    assert(n>=5 && n<=12);
    unsigned stones=0,friendship=0;
    for(unsigned i=0;i<n;i++){
        assert(opts[i].from==133);
        if(opts[i].kind==PK_EVO_STONE)stones++;
        if(opts[i].kind==PK_EVO_FRIENDSHIP)friendship++;
    }
    assert(stones==3 && friendship==2);
    assert(pk_evo_options(133,NULL,0)==n);
    pk_evo_rule_t tiny[1]={{0}};
    assert(pk_evo_options(133,tiny,1)==n && tiny[0].to==0);
    assert(pk_evo_level_eligible(161,162,15));
    assert(!pk_evo_level_eligible(161,162,14));
    assert(pk_evo_level_eligible(4,5,16));
    assert(pk_evo_level_eligible(5,6,36));
    assert(!pk_evo_level_eligible(133,134,100)); /* Water Stone */
    assert(!pk_evo_level_eligible(93,94,100)); /* trade */
    pk_profile_t p={0},next={0};
    pk_profile_init(&p);
    assert(pk_team_add(&p,161,true));
    p.team[0].level=16;p.team[0].xp=123;
    assert(pk_evo_prepare_level(&p,0,162,&next));
    assert(p.team[0].species==161);
    assert(next.team[0].species==162);
    assert(next.team[0].level==16 && next.team[0].xp==123);
    assert(next.team[0].shiny==1 && pk_dex_has(&next,162,true));
    assert(!pk_evo_prepare_level(&p,0,134,&next));
    puts("Pokemon base-only and explicit level evolution rules PASS");
}
