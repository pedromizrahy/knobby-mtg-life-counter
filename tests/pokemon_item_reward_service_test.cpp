#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "SPIFFS.h"
#include "Preferences.h"
#include "../knobby/src/pokemon_item_reward_service.h"
#include "../knobby/src/pokemon_item_enrollment.h"
#include "../knobby/src/pokemon_file_store.h"
#include "../knobby/src/pokemon_starter_store.h"
MockSPIFFS SPIFFS;
std::map<std::string,std::vector<uint8_t>> Preferences::data;
bool Preferences::fail_next_write=false;
int main(){
    const uint32_t id=184199U;
    pk_item_ledger_t out={};
    assert(pk_item_reward_sync_starter(id,&out)==PK_ITEM_REWARD_BLOCKED);
    pk_starter_state_t selected={};
    selected.current={161,PK_TIER_COMMON,0};
    selected.reveal_count=1;selected.revealed_species[0]=161;
    assert(pk_starter_store_create(id,&selected));
    assert(pk_file_create_selected_starter(id,161,false));
    assert(pk_item_reward_sync_starter(id,&out)==PK_ITEM_REWARD_BLOCKED);
    selected.kept=true;
    assert(pk_starter_store_update(id,&selected));
    assert(pk_item_enroll_confirmed_starter(id,0x1234567812345678ULL,&out)==PK_ITEM_ENROLL_CREATED);
    assert(pk_item_reward_sync_starter(id,&out)==PK_ITEM_REWARD_UNCHANGED);
    assert(pk_item_reward_claim_starter(id,0,PK_ITEM_WATER_STONE,&out)==PK_ITEM_REWARD_BLOCKED);
    pk_profile_t p={};
    assert(pk_file_inspect(id,&p)==PK_STORE_VALID);
    p.team[0].level=30;
    assert(pk_file_save(id,&p));
    assert(pk_item_reward_claim_starter(id,0,PK_ITEM_LINK_CABLE,&out)==PK_ITEM_REWARD_BLOCKED);
    assert(pk_item_reward_claim_starter(id,0,PK_ITEM_WATER_STONE,&out)==PK_ITEM_REWARD_CLAIMED);
    assert(out.count[PK_ITEM_WATER_STONE]==1U);
    assert(out.mons[0].claimed_mask==1U);
    assert(pk_item_reward_claim_starter(id,0,PK_ITEM_FIRE_STONE,&out)==PK_ITEM_REWARD_BLOCKED);
    assert(pk_item_reward_sync_starter(id,&out)==PK_ITEM_REWARD_UNCHANGED);
    p.team[0].level=75;
    assert(pk_file_save(id,&p));
    assert(pk_item_reward_sync_starter(id,&out)==PK_ITEM_REWARD_SYNCED);
    assert(out.mons[0].earned_mask==7);
    assert(pk_item_reward_claim_starter(id,1,PK_ITEM_LINK_CABLE,&out)==PK_ITEM_REWARD_CLAIMED);
    assert(pk_item_reward_claim_starter(id,2,PK_ITEM_FIRE_STONE,&out)==PK_ITEM_REWARD_CLAIMED);
    assert(out.count[PK_ITEM_FIRE_STONE]==1);
    assert(out.count[PK_ITEM_LINK_CABLE]==1);
    assert(pk_item_reward_claim_starter(id,2,PK_ITEM_METAL_COAT,&out)==PK_ITEM_REWARD_BLOCKED);
    pk_item_ledger_t readback={};
    assert(pk_item_store_inspect(id,&readback,NULL)==PK_ITEM_STORE_VALID);
    assert(readback.count[PK_ITEM_FIRE_STONE]==1);
    assert(readback.count[PK_ITEM_WATER_STONE]==1);
    /* Simulate an interrupted sidecar write: fail closed, never
       re-credit from the older generation. */
    SPIFFS.fail_write=true;
    assert(pk_item_reward_sync_starter(id,&out)==PK_ITEM_REWARD_UNCHANGED);
    SPIFFS.fail_write=false;
    p.team[0].level=80;
    assert(pk_file_save(id,&p));
    /* All milestones already earned, no new write. */
    assert(pk_item_reward_sync_starter(id,&out)==PK_ITEM_REWARD_UNCHANGED);
    puts("Durable starter milestone sync and once-only choice tests PASS");
}
