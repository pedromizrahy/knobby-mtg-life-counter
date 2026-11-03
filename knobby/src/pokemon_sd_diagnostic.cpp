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
