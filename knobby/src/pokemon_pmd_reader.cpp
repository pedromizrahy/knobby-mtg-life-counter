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


/* Optional USB transfer, only /mons/p778.bin, never overwrites another file.
 * Sender provides size and CRC32 and waits for a '#' after every 1024 bytes.
 * Stage in /mons/p778.part, then rename only after successful verification.
 */
static uint32_t pk_pmd_crc_step(uint32_t crc,const uint8_t *data,size_t n)
{
    for(size_t i=0;i<n;++i) {
        crc^=data[i];
        for(unsigned j=0;j<8;++j)
            crc=(crc>>1)^(0xEDB88320UL & (0U-(crc&1U)));
    }
    return crc;
}

extern "C" void pk_pmd_put_mimikyu(unsigned long input_size,
                                   unsigned long input_crc)
{
    const uint32_t size=(uint32_t)input_size;
    const uint32_t expected=(uint32_t)input_crc;
    if(size<7 || size>3UL*1024UL*1024UL || !board) {
        Serial.println("[Pokemon] PMD PUT ERR: invalid size or board.");
        return;
    }
    int clk=-1,cmd=-1,d0=-1;
    if(board==&board_k518) {clk=4;cmd=3;d0=5;}
    else if(board==&board_k718) {clk=39;cmd=38;d0=40;}
    else {
        Serial.println("[Pokemon] PMD PUT ERR: unsupported board.");
        return;
    }
    if(!SD_MMC.setPins(clk,cmd,d0) ||
       !SD_MMC.begin("/sdcard",true,false)) {
        Serial.println("[Pokemon] PMD PUT ERR: SD mount failed; no format.");
        return;
    }
    const char *dest="/mons/p778.bin";
    const char *part="/mons/p778.part";
    bool ok=false;
    if(SD_MMC.cardType()==CARD_NONE || SD_MMC.exists(dest)) {
        Serial.println("[Pokemon] PMD PUT ERR: SD missing or destination already exists.");
        SD_MMC.end();return;
    }
    if(!SD_MMC.exists("/mons") && !SD_MMC.mkdir("/mons")) {
        Serial.println("[Pokemon] PMD PUT ERR: cannot create /mons.");
        SD_MMC.end();return;
    }
    /* .part is exclusively owned by this command. Remove stale incomplete
     * transfers, but never touch the destination or other files. */
    if(SD_MMC.exists(part) && !SD_MMC.remove(part)) {
        Serial.println("[Pokemon] PMD PUT ERR: stale .part cannot be removed.");
        SD_MMC.end();return;
    }
    File out=SD_MMC.open(part,FILE_WRITE);
    if(!out) {
        Serial.println("[Pokemon] PMD PUT ERR: cannot create staging file.");
        SD_MMC.end();return;
    }
    uint8_t buf[256];
    uint32_t crc=0xFFFFFFFFUL,remaining=size;
    const char *failure="none";
    Serial.println("PMD_READY");
    Serial.flush(); /* Ensure host sees readiness before binary data begins. */
    while(remaining) {
        const size_t want=remaining>sizeof(buf)?sizeof(buf):remaining;
        size_t got=0;
        const uint32_t start=millis();
        while(got<want && millis()-start<20000UL) {
            const int available=Serial.available();
            if(available>0) {
                const int b=Serial.read();
                if(b>=0)buf[got++]=(uint8_t)b;
            } else delay(1);
        }
        if(got!=want) {
            failure="serial_timeout";
            Serial.printf("[Pokemon] PMD PUT DIAG timeout got=%u want=%u remaining=%lu\n",
                          (unsigned)got,(unsigned)want,(unsigned long)remaining);
            break;
        }
        if(size==remaining && (want<4 || memcmp(buf,"TPK2",4)!=0)) {
            failure="invalid_first_packet";
            Serial.printf("[Pokemon] PMD PUT DIAG first_bytes=%02x %02x %02x %02x (expected 54 50 4b 32)\n",
                buf[0],buf[1],buf[2],buf[3]);
            break;
        }
        if(out.write(buf,got)!=got) {failure="sd_write_failed";break;}
        crc=pk_pmd_crc_step(crc,buf,got);
        remaining-=got;
        Serial.println("#");
    }
    out.flush();
    out.close();
    crc^=0xFFFFFFFFUL;
    if(remaining==0 && crc==expected && !SD_MMC.exists(dest)) {
        ok=SD_MMC.rename(part,dest);
    }
    if(!ok && remaining==0 && crc!=expected)failure="crc_mismatch";
    if(!ok && remaining==0 && crc==expected)failure="rename_failed";
    if(!ok)SD_MMC.remove(part);
    SD_MMC.end();
    if(ok)Serial.printf("[Pokemon] PMD PUT DONE size=%lu crc32=%08lx\n",
                        (unsigned long)size,(unsigned long)crc);
    else Serial.printf("[Pokemon] PMD PUT ERR reason=%s remaining=%lu crc32=%08lx expected=%08lx; staging removed\n",
                       failure,(unsigned long)remaining,(unsigned long)crc,(unsigned long)expected);
}
