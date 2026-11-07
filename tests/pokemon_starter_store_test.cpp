#include <cassert>
#include <cstdio>
#include "SPIFFS.h"
#include "../knobby/src/pokemon_starter_store.h"
MockSPIFFS SPIFFS;
static pk_starter_state_t one(uint16_t species){
    pk_starter_state_t s={};
    s.current={species,PK_TIER_COMMON,0};
    s.reveal_count=1;s.revealed_species[0]=species;
    return s;
}
int main(){
    pk_starter_state_t found={};
    assert(pk_starter_store_inspect(25,&found)==PK_STARTER_STORE_MISSING);
    auto a=one(1);
    assert(pk_starter_store_create(25,&a));
    assert(!pk_starter_store_create(25,&a));
    assert(pk_starter_store_inspect(25,&found)==PK_STARTER_STORE_VALID);
    assert(found.current.species==1 && found.reveal_count==1);
    auto b=a;
    b.current.species=4;b.revealed_species[1]=4;b.reveal_count=2;
    assert(pk_starter_store_update(25,&b));
    assert(pk_starter_store_inspect(25,&found)==PK_STARTER_STORE_VALID);
    assert(found.current.species==4 && found.reveal_count==2);
    assert(!pk_starter_store_update(25,&a));
    auto fake=b;fake.revealed_species[0]=20;
    assert(!pk_starter_store_update(25,&fake));
    auto keep=b;keep.kept=true;
    assert(pk_starter_store_update(25,&keep));
    assert(!pk_starter_store_update(25,&keep));
    assert(pk_starter_store_inspect(25,&found)==PK_STARTER_STORE_VALID);
    assert(found.kept && found.current.species==4);
    auto before=SPIFFS.files;
    SPIFFS.fail_write=true;
    assert(!pk_starter_store_create(26,&a));
    SPIFFS.fail_write=false;
    assert(pk_starter_store_inspect(26,&found)==PK_STARTER_STORE_CORRUPT);
    assert(!pk_starter_store_create(26,&a));
    assert(SPIFFS.files!=before); /* interrupted creation left a marker */
    SPIFFS.files["/psr_00000019b.bin"][0]^=0xFF;
    assert(pk_starter_store_inspect(25,&found)==PK_STARTER_STORE_CORRUPT);
    auto corrupted=SPIFFS.files;
    assert(!pk_starter_store_update(25,&keep));
    assert(SPIFFS.files==corrupted);
    SPIFFS.mount_ok=false;
    assert(pk_starter_store_inspect(27,&found)==PK_STARTER_STORE_ERROR);
    assert(!pk_starter_store_create(27,&a));
    /* Owner test account: explicitly reset only unkept starter draws.
     * Other players and confirmed KEEP decisions are preserved. */
    SPIFFS.mount_ok=true;
    auto mine=one(162);
    assert(pk_starter_store_create(184199U,&mine));
    auto other=one(25);
    assert(pk_starter_store_create(1234U,&other));
    assert(!pk_starter_store_reset_test(1234U));
    assert(pk_starter_store_reset_test(184199U));
    assert(pk_starter_store_inspect(184199U,&found)==PK_STARTER_STORE_MISSING);
    assert(pk_starter_store_inspect(1234U,&found)==PK_STARTER_STORE_VALID);
    assert(found.current.species==25);
    assert(pk_starter_store_create(184199U,&mine));
    assert(pk_starter_store_reset_test(184199U));
    auto committed=mine;
    committed.kept=true;
    assert(pk_starter_store_create(184199U,&mine));
    assert(pk_starter_store_update(184199U,&committed));
    assert(!pk_starter_store_reset_test(184199U));
    assert(pk_starter_store_inspect(184199U,&found)==PK_STARTER_STORE_VALID);
    assert(found.kept);
    std::puts("Starter durable fail-closed slot tests passed");
}
