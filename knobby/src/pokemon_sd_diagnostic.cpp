#include "pokemon_sd_diagnostic.h"
#include "../board_detect.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

/* Pin assignments verified against Guition-JC/Guition:
 * components/board/guition_boards/jc3636k518/board_devices.yaml
 * components/board/guition_boards/jc3636k718/board_devices.yaml.
 * Use only CLK/CMD/D0 in 1-bit mode; leave WiFi, Magic, NVS and SPIFFS alone.
 * All operations below are explicitly read-only; NEVER format the card.
 */
extern "C" void pk_sd_status_read_only(void)
{
    if (!board) {
        Serial.println("[Pokemon] SD FAIL: board not detected.");
        return;
    }
    int clk=-1,cmd=-1,d0=-1;
    if(board==&board_k518) {
        clk=4;cmd=3;d0=5;
    } else if(board==&board_k718) {
        clk=39;cmd=38;d0=40;
    } else {
        Serial.println("[Pokemon] SD FAIL: unsupported board.");
        return;
    }

    Serial.printf("[Pokemon] SD CHECK board=%s CLK=%d CMD=%d D0=%d (1-bit)\n",
                  board->name,clk,cmd,d0);
    Serial.println("[Pokemon] SD READ ONLY: no format, no file changes.");
    if(!SD_MMC.setPins(clk,cmd,d0)) {
        Serial.println("[Pokemon] SD FAIL: cannot configure pins.");
        return;
    }
    /* Third argument explicitly disables automatic formatting on failure.
     * Other storage backends (SPIFFS/NVS) are never accessed here. */
    if(!SD_MMC.begin("/sdcard",true,false)) {
        Serial.println("[Pokemon] SD FAIL: cannot mount; card was NOT formatted.");
        return;
    }

    const uint8_t type=SD_MMC.cardType();
    if(type==CARD_NONE) {
        Serial.println("[Pokemon] SD FAIL: no card detected.");
        SD_MMC.end();
        return;
    }
    const char *type_name=type==CARD_SD ? "SDSC" :
        (type==CARD_SDHC ? "SDHC/SDXC" : (type==CARD_MMC ? "MMC":"UNKNOWN"));
    const unsigned long long capacity=(unsigned long long)SD_MMC.cardSize();
    const unsigned long long total=(unsigned long long)SD_MMC.totalBytes();
    const unsigned long long used=(unsigned long long)SD_MMC.usedBytes();
    Serial.printf("[Pokemon] SD OK type=%s capacity=%llu total=%llu used=%llu bytes\n",
                  type_name,capacity,total,used);
    Serial.printf("[Pokemon] SD CONTENT mons_dir=%s mimikyu_tpk2=%s\n",
                  SD_MMC.exists("/mons")?"YES":"NO",
                  SD_MMC.exists("/mons/p778.bin")?"YES":"NO");
    SD_MMC.end();
    Serial.println("[Pokemon] SD CHECK COMPLETE: unmounted; no files modified.");
}


/* Directory listing is strictly bounded to avoid long blocking I/O on the
 * Magic dial. Never read file contents, create folders or write to the SD.
 * File names can be relative or absolute depending on Arduino FS version. */
static const unsigned PK_SD_LIST_MAX_ENTRIES=100;
static const unsigned PK_SD_LIST_MAX_DEPTH=2;
static unsigned pk_sd_list_count=0;
static bool pk_sd_list_truncated=false;

static void pk_sd_list_directory(const char *path,unsigned depth)
{
    if(pk_sd_list_count>=PK_SD_LIST_MAX_ENTRIES) {
        pk_sd_list_truncated=true;
        return;
    }
    File dir=SD_MMC.open(path,FILE_READ);
    if(!dir || !dir.isDirectory()) {
        Serial.printf("[Pokemon] SD LIST: cannot read directory %s\n",path);
        if(dir)dir.close();
        return;
    }

    File entry=dir.openNextFile();
    while(entry) {
        if(pk_sd_list_count>=PK_SD_LIST_MAX_ENTRIES) {
            pk_sd_list_truncated=true;
            entry.close();
            break;
        }
        const bool is_dir=entry.isDirectory();
        const String name=String(entry.name());
        String full_path;
        if(name.startsWith("/"))full_path=name;
        else {
            full_path=path;
            if(!full_path.endsWith("/"))full_path+="/";
            full_path+=name;
        }
        ++pk_sd_list_count;
        if(is_dir)
            Serial.printf("[Pokemon] SD DIR  %s\n",full_path.c_str());
        else
            Serial.printf("[Pokemon] SD FILE %s size=%llu bytes\n",
                full_path.c_str(),(unsigned long long)entry.size());
        entry.close();

        /* Never recurse beyond two subdirectory levels. */
        if(is_dir && depth<PK_SD_LIST_MAX_DEPTH &&
           pk_sd_list_count<PK_SD_LIST_MAX_ENTRIES)
            pk_sd_list_directory(full_path.c_str(),depth+1);
        else if(is_dir && depth>=PK_SD_LIST_MAX_DEPTH)
            pk_sd_list_truncated=true;

        if(pk_sd_list_count>=PK_SD_LIST_MAX_ENTRIES) {
            pk_sd_list_truncated=true;
            break;
        }
        entry=dir.openNextFile();
    }
    dir.close();
}

extern "C" void pk_sd_list_read_only(void)
{
    if(!board) {
        Serial.println("[Pokemon] SD LIST FAIL: board not detected.");
        return;
    }
    int clk=-1,cmd=-1,d0=-1;
    if(board==&board_k518) {
        clk=4;cmd=3;d0=5;
    } else if(board==&board_k718) {
        clk=39;cmd=38;d0=40;
    } else {
        Serial.println("[Pokemon] SD LIST FAIL: unsupported board.");
        return;
    }
    Serial.printf("[Pokemon] SD LIST board=%s max=%u depth=%u (read-only)\n",
                  board->name,PK_SD_LIST_MAX_ENTRIES,PK_SD_LIST_MAX_DEPTH);
    if(!SD_MMC.setPins(clk,cmd,d0) ||
       !SD_MMC.begin("/sdcard",true,false)) {
        Serial.println("[Pokemon] SD LIST FAIL: cannot mount; NOT formatted.");
        return;
    }
    if(SD_MMC.cardType()==CARD_NONE) {
        Serial.println("[Pokemon] SD LIST FAIL: no card detected.");
        SD_MMC.end();
        return;
    }
    const unsigned long long total=(unsigned long long)SD_MMC.totalBytes();
    const unsigned long long used=(unsigned long long)SD_MMC.usedBytes();
    Serial.printf("[Pokemon] SD LIST SPACE total=%llu used=%llu free=%llu bytes\n",
                  total,used,total>=used?total-used:0ULL);
    pk_sd_list_count=0;
    pk_sd_list_truncated=false;
    pk_sd_list_directory("/",0);
    Serial.printf("[Pokemon] SD LIST DONE entries=%u%s\n",pk_sd_list_count,
                  pk_sd_list_truncated?" TRUNCATED (more files may exist)":"");
    SD_MMC.end();
    Serial.println("[Pokemon] SD LIST READ-ONLY: unmounted, nothing changed.");
}
