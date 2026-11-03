#include <cassert>
#include <cstdio>
#include "SPIFFS.h"
#include "../knobby/src/pokemon_file_store.h"
MockSPIFFS SPIFFS;
int main() {
    pk_profile_t p={};
    assert(pk_file_inspect(42,&p)==PK_STORE_MISSING);
    assert(pk_file_create_starter(42));
    assert(pk_file_inspect(42,&p)==PK_STORE_VALID);
    assert(p.team_count==1 && p.team[0].species==778);
    assert(!pk_file_create_starter(42));
    pk_reward_t r={};
    assert(pk_award_game(&p,100,true,&r));
    assert(pk_file_save(42,&p));
    pk_profile_t loaded={};
    assert(pk_file_inspect(42,&loaded)==PK_STORE_VALID);
    assert(loaded.wins==1 && loaded.battles==1);
    /* The most recent slot is corrupted; previous slot survives. */
    SPIFFS.files["/pkf_0000002ab.bin"][0]^=0xff;
    assert(pk_file_inspect(42,&loaded)==PK_STORE_VALID);
    assert(loaded.battles==0);
    assert(pk_file_save(42,&loaded));
    assert(pk_file_inspect(42,&loaded)==PK_STORE_VALID);
    /* Both corrupted slots: never reset or overwrite. */
    SPIFFS.files["/pkf_0000002aa.bin"][0]^=0xff;
    SPIFFS.files["/pkf_0000002ab.bin"][0]^=0xff;
    auto a=SPIFFS.files["/pkf_0000002aa.bin"];
    auto b=SPIFFS.files["/pkf_0000002ab.bin"];
    assert(pk_file_inspect(42,&loaded)==PK_STORE_CORRUPT);
    assert(!pk_file_create_starter(42));
    assert(!pk_file_save(42,&loaded));
    assert(SPIFFS.files["/pkf_0000002aa.bin"]==a &&
           SPIFFS.files["/pkf_0000002ab.bin"]==b);
    SPIFFS.fail_write=true;
    assert(!pk_file_create_starter(53));
    SPIFFS.fail_write=false;
    assert(pk_file_inspect(53,&p)==PK_STORE_CORRUPT); /* partial slot stays */
    assert(!pk_file_create_starter(53)); /* fail closed */
    SPIFFS.mount_ok=false;
    assert(pk_file_inspect(54,&p)==PK_STORE_ERROR);
    assert(!pk_file_create_starter(54));
    /* Reserved diagnostic profile: live-format, alternating copies,
       simulated corruption recovery, and cleanup without touching others. */
    SPIFFS.mount_ok=true;
    const auto others=SPIFFS.files;
    assert(pk_file_profile_selftest());
    assert(SPIFFS.files==others);
    assert(pk_file_profile_selftest());
    assert(SPIFFS.files==others);
    /* Both public APIs refuse reserved diagnostic IDs. */
    pk_profile_t reserved={};
    assert(pk_file_inspect(0xFFFFFFFDUL,&reserved)==PK_STORE_ERROR);
    assert(!pk_file_create_starter(0xFFFFFFFDUL));
    assert(!pk_file_save(0xFFFFFFFDUL,&reserved));
    /* An interrupted diagnostic must never overwrite or erase its residue. */
    SPIFFS.files["/pkf_fffffffda.bin"]={0x22};
    auto before=SPIFFS.files;
    assert(!pk_file_profile_selftest());
    assert(SPIFFS.files==before);
    SPIFFS.files.erase("/pkf_fffffffda.bin");
    SPIFFS.fail_write=true;
    assert(!pk_file_profile_selftest());
    SPIFFS.fail_write=false;
    assert(SPIFFS.files==others);
    SPIFFS.fail_remove=true;
    assert(!pk_file_profile_selftest());
    SPIFFS.fail_remove=false;
    assert(SPIFFS.exists("/pkf_fffffffda.bin") ||
           SPIFFS.exists("/pkf_fffffffdb.bin"));
    before=SPIFFS.files;
    assert(!pk_file_profile_selftest());
    assert(SPIFFS.files==before);
    SPIFFS.files.erase("/pkf_fffffffda.bin");
    SPIFFS.files.erase("/pkf_fffffffdb.bin");
    assert(SPIFFS.files==others);
    std::puts("Pokemon SPIFFS dual-slot tests passed");
}
