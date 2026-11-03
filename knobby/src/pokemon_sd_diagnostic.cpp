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


/* Cleanup is opt-in and constrained to seven known demonstration folders.
 * No root-wide erase, no formatting, no touch of fonts/mons/scenes/audio/assets.
 * PREVIEW never writes; CONFIRM performs irreversible removals on the SD.
 */
static const char *const pk_demo_folders[]={
    "/clockbg","/weather","/aida64","/night7","/pic","/txt","/mjpeg"
};
static const size_t PK_DEMO_FOLDER_COUNT=
    sizeof(pk_demo_folders)/sizeof(pk_demo_folders[0]);
static const unsigned PK_SD_CLEAN_MAX_ENTRIES=2000;
static const unsigned PK_SD_CLEAN_MAX_DEPTH=6;

static bool pk_sd_cleanup_mount()
{
    if(!board) {
        Serial.println("[Pokemon] SD CLEAN FAIL: board not detected.");
        return false;
    }
    int clk=-1,cmd=-1,d0=-1;
    if(board==&board_k518) {
        clk=4;cmd=3;d0=5;
    } else if(board==&board_k718) {
        clk=39;cmd=38;d0=40;
    } else {
        Serial.println("[Pokemon] SD CLEAN FAIL: unsupported board.");
        return false;
    }
    if(!SD_MMC.setPins(clk,cmd,d0) ||
       !SD_MMC.begin("/sdcard",true,false)) {
        Serial.println("[Pokemon] SD CLEAN FAIL: mount failed; card NOT formatted.");
        return false;
    }
    if(SD_MMC.cardType()==CARD_NONE) {
        Serial.println("[Pokemon] SD CLEAN FAIL: card missing.");
        SD_MMC.end();
        return false;
    }
    return true;
}

static bool pk_sd_cleanup_is_safe_path(const String &path,const String &root)
{
    if(!path.startsWith(root+"/"))return false;
    if(path.indexOf("..")>=0 || path.indexOf("//")>=0 ||
       path.indexOf((char)92)>=0)return false;
    return true;
}

struct PkSdCleanupStats {
    unsigned files=0;
    unsigned directories=0;
    unsigned errors=0;
    unsigned entries=0;
    unsigned long long file_bytes=0;
    bool truncated=false;
};

/* Traverse with a fixed operation cap and reject all paths outside the exact
 * selected root. This is NOT a general-purpose recursive delete routine. */
static void pk_sd_cleanup_walk(const String &dir_path,const String &root,
                               unsigned depth,bool erase,PkSdCleanupStats &stats)
{
    if(depth>PK_SD_CLEAN_MAX_DEPTH || stats.entries>=PK_SD_CLEAN_MAX_ENTRIES) {
        stats.truncated=true;
        return;
    }
    File dir=SD_MMC.open(dir_path.c_str(),FILE_READ);
    if(!dir || !dir.isDirectory()) {
        ++stats.errors;
        if(dir)dir.close();
        return;
    }
    File item=dir.openNextFile();
    while(item) {
        if(stats.entries>=PK_SD_CLEAN_MAX_ENTRIES) {
            stats.truncated=true;
            item.close();
            break;
        }
        const String name=String(item.name());
        String child;
        if(name.startsWith("/"))child=name;
        else child=dir_path+"/"+name;
        const bool is_dir=item.isDirectory();
        const unsigned long long bytes=(unsigned long long)item.size();
        item.close();
        if(!pk_sd_cleanup_is_safe_path(child,root)) {
            ++stats.errors;
            Serial.printf("[Pokemon] SD CLEAN BLOCKED unexpected path %s\\n",child.c_str());
            break;
        }
        ++stats.entries;
        if(is_dir) {
            pk_sd_cleanup_walk(child,root,depth+1,erase,stats);
            if(erase && !stats.truncated) {
                if(SD_MMC.rmdir(child.c_str()))++stats.directories;
                else {
                    ++stats.errors;
                    Serial.printf("[Pokemon] SD CLEAN FAIL rmdir %s\\n",child.c_str());
                }
            } else if(!erase) ++stats.directories;
        } else {
            if(erase) {
                if(SD_MMC.remove(child.c_str())) {
                    ++stats.files;
                    stats.file_bytes+=bytes;
                } else {
                    ++stats.errors;
                    Serial.printf("[Pokemon] SD CLEAN FAIL remove %s\\n",child.c_str());
                }
            } else {
                ++stats.files;
                stats.file_bytes+=bytes;
            }
        }
        if(stats.truncated)break;
        item=dir.openNextFile();
    }
    dir.close();
}

/* Each folder is shown individually. Only exact allowlisted roots are erased.
 * Preview cannot imply deletion has occurred. */
static void pk_sd_cleanup_run(bool erase)
{
    Serial.println(erase ?
        "[Pokemon] SD CLEAN CONFIRM: deleting allowlisted factory demo folders." :
        "[Pokemon] SD CLEAN PREVIEW: no files will be changed.");
    Serial.println("[Pokemon] SD CLEAN PRESERVE: /fonts, /mons, /scenes, /audio, /assets, and all other roots.");
    if(erase)
        Serial.println("[Pokemon] SD CLEAN WARNING: irreversible without external backup.");
    if(!pk_sd_cleanup_mount())return;

    const unsigned long long used_before=(unsigned long long)SD_MMC.usedBytes();
    PkSdCleanupStats total;
    for(size_t i=0;i<PK_DEMO_FOLDER_COUNT;++i) {
        const String root=pk_demo_folders[i];
        if(!SD_MMC.exists(root.c_str())) {
            Serial.printf("[Pokemon] SD CLEAN SKIP %s (absent)\\n",root.c_str());
            continue;
        }
        PkSdCleanupStats s;
        pk_sd_cleanup_walk(root,root,0,erase,s);
        if(erase && !s.truncated) {
            if(!SD_MMC.rmdir(root.c_str())) {
                ++s.errors;
                Serial.printf("[Pokemon] SD CLEAN FAIL root %s not empty or inaccessible\\n",root.c_str());
            } else ++s.directories;
        }
        total.files+=s.files;
        total.directories+=s.directories;
        total.errors+=s.errors;
        total.entries+=s.entries;
        total.file_bytes+=s.file_bytes;
        total.truncated=total.truncated||s.truncated;
        Serial.printf("[Pokemon] SD CLEAN %s %s files=%u dirs=%u file_bytes=%llu errors=%u%s\\n",
            erase?"RESULT":"PREVIEW",root.c_str(),s.files,s.directories,
            s.file_bytes,s.errors,s.truncated?" TRUNCATED":"");
        if(s.truncated) {
            Serial.println("[Pokemon] SD CLEAN LIMIT reached: remaining folders NOT processed.");
            break;
        }
    }
    const unsigned long long used_after=(unsigned long long)SD_MMC.usedBytes();
    Serial.printf("[Pokemon] SD CLEAN %s files=%u dirs=%u file_bytes=%llu errors=%u%s\\n",
        erase?"COMPLETE":"PREVIEW COMPLETE",
        total.files,total.directories,total.file_bytes,total.errors,
        total.truncated?" TRUNCATED (incomplete)":"");
    Serial.printf("[Pokemon] SD CLEAN USED before=%llu after=%llu bytes\\n",used_before,used_after);
    SD_MMC.end();
    Serial.println(erase ?
        "[Pokemon] SD CLEAN unmounted. Review errors and run PK SD STATUS." :
        "[Pokemon] SD CLEAN preview unmounted; no files modified.");
}

extern "C" void pk_sd_cleanup_preview(void)
{
    pk_sd_cleanup_run(false);
}

extern "C" void pk_sd_cleanup_confirm(void)
{
    pk_sd_cleanup_run(true);
}
