#include <cassert>
#include <cstdio>
#include <vector>
#include "SPIFFS.h"
#include "../knobby/src/pokemon_spiffs_selftest.h"
MockSPIFFS SPIFFS;
int main() {
    SPIFFS.files["/pgq_keep.bin"]={1,2,3};
    SPIFFS.files["/pga_keep.bin"]={4,5,6,7};
    const auto untouched=SPIFFS.files;
    assert(pk_spiffs_selftest());
    assert(SPIFFS.files==untouched);
    assert(!SPIFFS.exists("/pkt_spiffs_01.bin"));
    assert(pk_spiffs_selftest());
    assert(SPIFFS.files==untouched);

    /* Never overwrite or delete an existing reserved file. */
    SPIFFS.files["/pkt_spiffs_01.bin"]={0xaa,0xbb};
    auto existing=SPIFFS.files;
    assert(!pk_spiffs_selftest());
    assert(SPIFFS.files==existing);
    SPIFFS.files.erase("/pkt_spiffs_01.bin");

    SPIFFS.fail_write=true;
    assert(!pk_spiffs_selftest());
    SPIFFS.fail_write=false;
    assert(SPIFFS.files==untouched);

    SPIFFS.fail_remove=true;
    assert(!pk_spiffs_selftest());
    SPIFFS.fail_remove=false;
    assert(SPIFFS.exists("/pkt_spiffs_01.bin"));
    assert(!pk_spiffs_selftest()); /* don't touch interrupted diagnostic */
    SPIFFS.files.erase("/pkt_spiffs_01.bin");

    SPIFFS.capacity=128; /* too little reserved space */
    assert(!pk_spiffs_selftest());
    assert(SPIFFS.files==untouched);
    SPIFFS.capacity=3245681U;
    SPIFFS.mount_ok=false;
    assert(!pk_spiffs_selftest());
    assert(SPIFFS.files==untouched);
    std::puts("Pokemon SPIFFS hardware diagnostic mock tests passed");
}
