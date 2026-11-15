#include "pokemon_spiffs_selftest.h"
#include <SPIFFS.h>
#include <stdint.h>
#include <string.h>

/* This is NOT a user profile, so it does not consume a Playgroup ID.
 * Do not remove an unexpected preexisting file at the diagnostic path. */
static const char *TEST_PATH="/pkt_spiffs_01.bin";
static const uint8_t HEADER[12]={
    'P','K','T','1',0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80
};

static bool verify(const uint8_t *expected,size_t length)
{
    File f=SPIFFS.open(TEST_PATH,FILE_READ);
    if(!f)return false;
    uint8_t readback[sizeof(HEADER)];
    bool ok=length==sizeof(readback) &&
            f.size()==length &&
            f.read(readback,length)==(int)length &&
            memcmp(readback,expected,length)==0;
    f.close();
    return ok;
}
extern "C" bool pk_spiffs_selftest(void)
{
    if(!SPIFFS.begin(false) || SPIFFS.exists(TEST_PATH))
        return false;
    /* Mount/space checks are advisory. Actual write/read-back determines
     * success. Reserve a substantial margin for the existing Playgroup
     * pending queue and art cache, not just our tiny diagnostic file. */
    size_t total=SPIFFS.totalBytes();
    size_t used=SPIFFS.usedBytes();
    if(total==0 || used>total || (total-used)<(256U*1024U))
        return false;

    uint8_t second[sizeof(HEADER)];
    memcpy(second,HEADER,sizeof(second));
    second[4]^=0xA5U;
    bool result=false;
    bool created=false;
    do {
        File f=SPIFFS.open(TEST_PATH,FILE_WRITE);
        if(!f) break;
        created=true;
        size_t n=f.write(HEADER,sizeof(HEADER));
        f.flush();
        f.close();
        if(n!=sizeof(HEADER) || !verify(HEADER,sizeof(HEADER)))break;

        f=SPIFFS.open(TEST_PATH,FILE_WRITE);
        if(!f)break;
        n=f.write(second,sizeof(second));
        f.flush();
        f.close();
        if(n!=sizeof(second) || !verify(second,sizeof(second)))break;
        result=true;
    } while(false);

    /* Remove only the file that this invocation successfully created.
     * If cleanup fails, a subsequent run refuses to overwrite the file. */
    if(created && !SPIFFS.remove(TEST_PATH))result=false;
    return result;
}
