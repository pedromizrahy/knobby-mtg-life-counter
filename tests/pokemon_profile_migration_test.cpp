#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "Preferences.h"
#include "SPIFFS.h"
#include "../knobby/src/pokemon_profile_migration.h"
#include "../knobby/src/pokemon_profile_store.h"
#include "../knobby/src/pokemon_file_store.h"
std::map<std::string,std::vector<uint8_t>> Preferences::data;
bool Preferences::fail_next_write=false;
MockSPIFFS SPIFFS;
int main() {
  pk_profile_t old={},read={};
  assert(pk_profile_migrate_to_spiffs(0)==PK_MIGRATE_ERROR);
  assert(pk_profile_migrate_to_spiffs(42)==PK_MIGRATE_NOTHING);
  assert(pk_store_create_starter(42));
  assert(pk_store_inspect(42,&old)==PK_STORE_VALID);
  pk_reward_t reward={};
  assert(pk_award_game(&old,987,true,&reward));
  assert(pk_store_save(42,&old));
  auto nvs_before=Preferences::data;
  assert(pk_profile_migrate_to_spiffs(42)==PK_MIGRATE_COPIED);
  assert(Preferences::data==nvs_before);
  assert(pk_file_inspect(42,&read)==PK_STORE_VALID);
  assert(read.wins==1 && read.battles==1);
  assert(pk_profile_migrate_to_spiffs(42)==PK_MIGRATE_ALREADY);
  assert(Preferences::data==nvs_before);

  /* A different target is a conflict; never overwrite it. */
  assert(pk_award_game(&read,988,true,&reward));
  assert(pk_file_save(42,&read));
  auto file_before=SPIFFS.files;
  assert(pk_profile_migrate_to_spiffs(42)==PK_MIGRATE_CONFLICT);
  assert(SPIFFS.files==file_before);

  /* Missing NVS but existing SPIFFS is safe, not a new profile. */
  assert(pk_file_create_starter(50));
  assert(pk_profile_migrate_to_spiffs(50)==PK_MIGRATE_FILE_ONLY);
  /* Corrupt NVS must not silently reinitialize the target. */
  assert(pk_store_create_starter(60));
  Preferences::data.at("pk_companions:p0000003ca").at(0)^=0xff;
  assert(pk_profile_migrate_to_spiffs(60)==PK_MIGRATE_CONFLICT);

  /* A failed stage write leaves no published profile, preserves source. */
  assert(pk_store_create_starter(70));
  SPIFFS.fail_write=true;
  assert(pk_profile_migrate_to_spiffs(70)==PK_MIGRATE_ERROR);
  SPIFFS.fail_write=false;
  assert(pk_file_inspect(70,&read)==PK_STORE_MISSING);
  assert(SPIFFS.exists("/pkm_00000046.tmp"));
  assert(pk_profile_migrate_to_spiffs(70)==PK_MIGRATE_ERROR);
  assert(pk_store_inspect(70,&read)==PK_STORE_VALID);

  /* Storage failure must not be treated as a genuinely empty profile. */
  SPIFFS.mount_ok=false;
  assert(pk_profile_migrate_to_spiffs(80)==PK_MIGRATE_ERROR);
  std::puts("Pokemon explicit migration tests passed");
}
