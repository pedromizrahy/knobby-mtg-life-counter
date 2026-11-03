#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "Preferences.h"
#include "../knobby/src/pokemon_profile_store.h"
std::map<std::string,std::vector<uint8_t>> Preferences::data;
bool Preferences::fail_next_write=false;
static std::string a(uint32_t id) {
 char b[30];std::snprintf(b,sizeof(b),"pk_companions:p%08lx%c",
 (unsigned long)id,'a');return b;
}
static std::string b(uint32_t id) {
 char x[30];std::snprintf(x,sizeof(x),"pk_companions:p%08lx%c",
 (unsigned long)id,'b');return x;
}
int main() {
 const uint32_t id=42;
 pk_profile_t profile;
 assert(pk_store_inspect(id,&profile)==PK_STORE_MISSING);
 assert(!pk_store_save(id,&profile));
 assert(pk_store_create_starter(id));
 assert(pk_store_inspect(id,&profile)==PK_STORE_VALID);
 assert(profile.team_count==1 && profile.team[0].species==778);
 assert(!pk_store_create_starter(id));
 assert(pk_store_save(id,&profile));
 assert(Preferences::data.count(a(id)) && Preferences::data.count(b(id)));
 /* One damaged slot can be repaired from the latest valid one. */
 Preferences::data[b(id)].at(0)^=0xFF;
 assert(pk_store_inspect(id,&profile)==PK_STORE_VALID);
 assert(pk_store_save(id,&profile));
 assert(pk_store_inspect(id,&profile)==PK_STORE_VALID);
 /* Two unreadable slots must never reset or overwrite the player's data. */
 Preferences::data[a(id)].at(0)^=0xFF;
 Preferences::data[b(id)].at(0)^=0xFF;
 auto before_a=Preferences::data[a(id)];
 auto before_b=Preferences::data[b(id)];
 assert(pk_store_inspect(id,&profile)==PK_STORE_CORRUPT);
 assert(!pk_store_create_starter(id));
 assert(!pk_store_save(id,&profile));
 assert(Preferences::data[a(id)]==before_a &&
        Preferences::data[b(id)]==before_b);
 /* A failed first write never creates a valid profile. */
 Preferences::fail_next_write=true;
 assert(!pk_store_create_starter(53));
 assert(pk_store_inspect(53,&profile)==PK_STORE_MISSING);
 assert(pk_store_create_starter(53));
 assert(pk_store_inspect(53,&profile)==PK_STORE_VALID);
 assert(!pk_store_create_starter(0xFFFFFFFEUL));
 /* Legacy diagnostics still work with the guarded save routine. */
 assert(pk_store_selftest());
 assert(!Preferences::data.count(a(0xFFFFFFFEUL)));
 assert(!Preferences::data.count(b(0xFFFFFFFEUL)));
 std::puts("Pokemon NVS profile safety tests passed");
}
