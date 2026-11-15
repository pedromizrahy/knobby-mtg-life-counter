#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "SPIFFS.h"
#include "Preferences.h"
#include "../knobby/src/pokemon_item_enrollment.h"
#include "../knobby/src/pokemon_starter_store.h"
#include "../knobby/src/pokemon_file_store.h"
MockSPIFFS SPIFFS;
std::map<std::string,std::vector<uint8_t>> Preferences::data;
bool Preferences::fail_next_write=false;
static pk_starter_state_t choice(uint16_t species,bool kept){
    pk_starter_state_t s={};
    s.current={species,PK_TIER_COMMON,0};
    s.revealed_species[0]=species;s.reveal_count=1;s.kept=kept;
    return s;
}
int main(){
    pk_item_ledger_t result={};
    const auto pending=choice(161,false);
    assert(pk_starter_store_create(75,&pending));
    assert(pk_item_enroll_confirmed_starter(75,123,&result)==PK_ITEM_ENROLL_BLOCKED);
    assert(pk_item_store_inspect(75,NULL,NULL)==PK_ITEM_STORE_MISSING);
    assert(pk_file_create_selected_starter(75,161,false));
    assert(pk_item_enroll_confirmed_starter(75,123,&result)==PK_ITEM_ENROLL_BLOCKED);
    auto kept=pending;kept.kept=true;
    assert(pk_starter_store_update(75,&kept));
    assert(pk_item_enroll_confirmed_starter(75,0,&result)==PK_ITEM_ENROLL_BLOCKED);
    assert(pk_item_enroll_confirmed_starter(75,0xCAFEBABE12345678ULL,&result)==PK_ITEM_ENROLL_CREATED);
    assert(result.mons[0].instance_id==0xCAFEBABE12345678ULL);
    assert(result.count[PK_ITEM_WATER_STONE]==0);
    assert(pk_item_enroll_confirmed_starter(75,5678,&result)==PK_ITEM_ENROLL_ALREADY);
    assert(result.mons[0].instance_id==0xCAFEBABE12345678ULL);
    assert(pk_item_store_inspect(76,NULL,NULL)==PK_ITEM_STORE_MISSING);
    auto rejected=choice(162,false);
    assert(pk_starter_store_create(76,&rejected));
    assert(pk_file_create_selected_starter(76,161,false));
    rejected.kept=true;
    assert(pk_starter_store_update(76,&rejected));
    assert(pk_item_enroll_confirmed_starter(76,321,&result)==PK_ITEM_ENROLL_BLOCKED);
    puts("Confirmed starter stable identity enrollment tests PASS");
}
