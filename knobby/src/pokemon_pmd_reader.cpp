#include "pokemon_pmd_reader.h"
#include "../board_detect.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

/* Adapted from the TPK2 container structure used by socquique/TamaPoke
 * (sdmon.cpp, MIT). Original sprite artwork is NOT bundled.
 * Probe is read-only and validates a bounded file without allocating it.
 */
static bool pk_pmd_read_exact(File &f,uint8_t *dst,size_t n)
{
    return f.read(dst,n)==n;
}

extern "C" void pk_pmd_check_mimikyu_read_only(void)
{
    if(!board) {
        Serial.println("[Pokemon] PMD FAIL: board unavailable.");
        return;
    }
    int clk=-1,cmd=-1,d0=-1;
    if(board==&board_k518) { clk=4;cmd=3;d0=5; }
    else if(board==&board_k718) { clk=39;cmd=38;d0=40; }
    else {
        Serial.println("[Pokemon] PMD FAIL: unsupported board.");
        return;
    }
    Serial.println("[Pokemon] PMD CHECK MIMIKYU #778: /mons/p778.bin (read-only)");
    if(!SD_MMC.setPins(clk,cmd,d0) ||
       !SD_MMC.begin("/sdcard",true,false)) {
        Serial.println("[Pokemon] PMD FAIL: SD mount failed; no format.");
        return;
    }
    if(SD_MMC.cardType()==CARD_NONE) {
        Serial.println("[Pokemon] PMD FAIL: no card.");
        SD_MMC.end();
        return;
    }
    const char *path="/mons/p778.bin";
    if(!SD_MMC.exists(path)) {
        Serial.println("[Pokemon] PMD MISSING: /mons/p778.bin (asset not installed).");
        SD_MMC.end();
        return;
    }
    File f=SD_MMC.open(path,FILE_READ);
    if(!f || f.isDirectory()) {
        Serial.println("[Pokemon] PMD FAIL: cannot open TPK2 file.");
        if(f)f.close();
        SD_MMC.end();
        return;
    }

    const uint32_t size=(uint32_t)f.size();
    bool ok=size>=7 && size<=3UL*1024UL*1024UL;
    uint8_t header[7]={0};
    if(ok)ok=pk_pmd_read_exact(f,header,sizeof(header));
    if(ok)ok=memcmp(header,"TPK2",4)==0;
    const uint8_t action_count=header[4];
    const uint16_t palette_count=(uint16_t)header[5]|((uint16_t)header[6]<<8);
    if(ok)ok=action_count>0 && action_count<=12 &&
             palette_count>0 && palette_count<=256 &&
             7UL+(uint32_t)palette_count*2UL<=size;
    if(ok)ok=f.seek(7UL+(uint32_t)palette_count*2UL);
    unsigned seen_mask=0;
    unsigned frames_total=0;
    for(unsigned i=0;ok && i<action_count;++i) {
        const uint32_t pos=(uint32_t)f.position();
        uint8_t act[4];
        if(pos>size || size-pos<4 || !pk_pmd_read_exact(f,act,4)) {ok=false;break;}
        const unsigned id=act[0],w=act[1],h=act[2],frames=act[3];
        const uint32_t bytes=(uint32_t)frames*2UL+(uint32_t)w*h*frames;
        if(id>=12 || (seen_mask&(1U<<id)) ||
           !w || !h || !frames || frames>24 || 
           (uint32_t)f.position()>size || bytes>size-(uint32_t)f.position()) {
            ok=false;break;
        }
        seen_mask|=1U<<id;
        frames_total+=frames;
        Serial.printf("[Pokemon] PMD ACTION id=%u size=%ux%u frames=%u\n",
                      id,w,h,frames);
        ok=f.seek((uint32_t)f.position()+bytes);
    }
    if(ok)ok=(uint32_t)f.position()==size;
    Serial.printf("[Pokemon] PMD %s size=%lu actions=%u palette=%u frames=%u\n",
                  ok?"VALID":"INVALID",(unsigned long)size,
                  (unsigned)action_count,(unsigned)palette_count,frames_total);
    f.close();
    SD_MMC.end();
    Serial.println("[Pokemon] PMD CHECK COMPLETE: card unmounted; no files modified.");
}
