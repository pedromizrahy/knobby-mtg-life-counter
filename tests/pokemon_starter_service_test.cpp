#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "SPIFFS.h"
#include "Preferences.h"
#include "../knobby/src/pokemon_starter_service.h"
MockSPIFFS SPIFFS;
std::map<std::string,std::vector<uint8_t>> Preferences::data;
bool Preferences::fail_next_write=false;
static bool rng(void *ctx,uint32_t *value){
    uint32_t *s=(uint32_t*)ctx;
    *s=*s*1664525U+1013904223U;
    *value=*s;
    return true;
}
int main(){
    const pk_gacha_entry_t entries[]={
        {1,PK_TIER_COMMON,1},{4,PK_TIER_COMMON,1},
        {25,PK_TIER_RARE,1},{94,PK_TIER_RARE,1},
        {6,PK_TIER_EPIC,1},{9,PK_TIER_EPIC,1},
        {150,PK_TIER_LEGENDARY,1},{151,PK_TIER_LEGENDARY,1}
    };
    uint32_t seed=12345;
    pk_starter_state_t view={};
    auto r=pk_starter_service_open(77,entries,8,rng,&seed,&view);
    assert(r==PK_STARTER_REVEALED && view.reveal_count==1);
    uint16_t original=view.current.species;
    /* Simulated restart cannot produce a new first draw. */
    seed=8888;
    r=pk_starter_service_open(77,entries,8,rng,&seed,&view);
    assert(r==PK_STARTER_REVEALED && view.reveal_count==1);
    assert(view.current.species==original);
    r=pk_starter_service_reroll(77,entries,8,rng,&seed,&view);
    assert(r==PK_STARTER_REVEALED && view.reveal_count==2);
    uint16_t chosen=view.current.species;
    bool shiny=view.current.shiny!=0U;
    r=pk_starter_service_keep(77,&view);
    assert(r==PK_STARTER_ACCEPTED && view.kept);
    pk_profile_t p={};
    assert(pk_file_inspect(77,&p)==PK_STORE_VALID);
    assert(p.team_count==1 && p.team[0].species==chosen &&
           p.team[0].shiny==(uint8_t)shiny && p.team[0].level==1);
    assert(pk_starter_service_keep(77,&view)==PK_STARTER_ALREADY_OWNED);
    assert(pk_starter_service_reroll(77,entries,8,rng,&seed,&view)==PK_STARTER_BLOCKED);
    assert(pk_starter_service_open(77,entries,8,rng,&seed,&view)==PK_STARTER_ALREADY_OWNED);
    /* Legacy NVS blocks a new starter even if the starter store is empty. */
    assert(pk_store_create_starter(78));
    assert(pk_starter_service_open(78,entries,8,rng,&seed,&view)==PK_STARTER_BLOCKED);
    /* If profile creation fails after KEEP, read back the same accepted
       result on retry. Never restore rerolls. */
    assert(pk_starter_service_open(79,entries,8,rng,&seed,&view)==PK_STARTER_REVEALED);
    const uint16_t saved=view.current.species;
    pk_gacha_result_t picked={};
    assert(pk_starter_keep(&view,&picked));
    assert(pk_starter_store_update(79,&view));
    SPIFFS.fail_write=true;
    assert(pk_starter_service_keep(79,&view)==PK_STARTER_FAILED);
    SPIFFS.fail_write=false;
    assert(view.kept && view.current.species==saved);
    /* Partially written profile marker blocks recovery rather than reset. */
    assert(pk_starter_service_keep(79,&view)==PK_STARTER_BLOCKED);
    assert(pk_starter_store_inspect(79,&view)==PK_STARTER_STORE_VALID && view.kept);
    puts("Starter onboarding transaction tests PASS");
}
