#include "pokemon_tpk2_renderer.h"
#include "../board_detect.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <stdlib.h>
#include <string.h>

/* TamaPoke pack_pmd.py (MIT): TPK2, RGB565 LE palette, indexed pixels,
 * 0xff = transparent. LVGL 8-bit indexed images store 256 BGRA palette
 * entries followed by one byte/pixel. Art is read from user microSD only.
 * This loader touches no Magic state, SPIFFS, NVS or Playgroup session. */
namespace {
constexpr unsigned kMaxFrames = 12;
constexpr unsigned kMaxWidth = 96, kMaxHeight = 96;
constexpr size_t kMaxTotalBytes = 64 * 1024;
constexpr unsigned kPaletteBytes = 256 * 4;
struct Frame {
    lv_img_dsc_t image;
    uint8_t *bytes;
    uint16_t ms;
};
Frame frames[kMaxFrames] = {};
unsigned frame_count = 0;
bool loaded = false;

bool exact(File &file, void *dest, size_t size) {
    return file.read((uint8_t *)dest, size) == size;
}
uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
uint32_t rgba565(uint16_t c) {
    uint8_t r = (uint8_t)((((c >> 11) & 31) * 255 + 15) / 31);
    uint8_t g = (uint8_t)((((c >> 5) & 63) * 255 + 31) / 63);
    uint8_t b = (uint8_t)(((c & 31) * 255 + 15) / 31);
    return 0xff000000UL | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
bool read_frames(File &file) {
    const uint32_t file_size = (uint32_t)file.size();
    uint8_t header[7];
    if (file_size < 7 || file_size > 3UL*1024UL*1024UL ||
        !exact(file, header, sizeof(header)) || memcmp(header, "TPK2", 4))
        return false;
    unsigned acts = header[4], palette_count = le16(header+5);
    if (!acts || acts > 12 || !palette_count || palette_count > 255)
        return false;
    uint32_t colors[256] = {};
    for (unsigned p = 0; p < palette_count; ++p) {
        uint8_t raw[2];
        if (!exact(file, raw, 2)) return false;
        colors[p] = rgba565(le16(raw));
    }
    colors[255] = 0; /* 0xff is alpha-transparent */
    for (unsigned act = 0; act < acts; ++act) {
        uint8_t info[4];
        if (!exact(file, info, 4)) return false;
        const unsigned id = info[0], w = info[1], h = info[2], count = info[3];
        if (!w || !h || !count || count > 24) return false;
        const uint32_t pixels = (uint32_t)w * h;
        const uint32_t lengths = 2UL * count + pixels * count;
        const uint32_t pos = (uint32_t)file.position();
        if (pos > file_size || lengths > file_size-pos) return false;
        if (id != 0) {
            if (!file.seek(pos + lengths)) return false;
            continue;
        }
        if (count > kMaxFrames || w > kMaxWidth || h > kMaxHeight ||
            (kPaletteBytes + pixels) * count > kMaxTotalBytes) return false;
        uint16_t durations[kMaxFrames] = {};
        for (unsigned i=0; i<count; ++i) {
            uint8_t raw[2];
            if (!exact(file,raw,2)) return false;
            durations[i] = le16(raw);
        }
        for (unsigned i=0; i<count; ++i) {
            Frame &f=frames[i];
            f.bytes=(uint8_t *)malloc(kPaletteBytes + pixels);
            if (!f.bytes) return false;
            memcpy(f.bytes, colors, kPaletteBytes);
            if (!exact(file, f.bytes+kPaletteBytes, pixels)) return false;
            for (uint32_t p=0; p<pixels; ++p) {
                uint8_t idx=f.bytes[kPaletteBytes+p];
                if (idx != 255 && idx >= palette_count) return false;
            }
            memset(&f.image, 0, sizeof(f.image));
            f.image.header.always_zero=0;
            f.image.header.w=w;
            f.image.header.h=h;
            f.image.header.cf=LV_IMG_CF_INDEXED_8BIT;
            f.image.data_size=kPaletteBytes+pixels;
            f.image.data=f.bytes;
            f.ms=durations[i] < 70 ? 70 : durations[i];
        }
        frame_count=count;
        return true;
    }
    return false;
}
} // namespace

extern "C" void pk_tpk2_idle_release(void) {
    for (unsigned i=0; i<kMaxFrames; ++i) {
        free(frames[i].bytes);
        memset(&frames[i],0,sizeof(frames[i]));
    }
    frame_count=0;
    loaded=false;
}
extern "C" bool pk_tpk2_idle_load(void) {
    if (loaded) return true;
    pk_tpk2_idle_release();
    int clk=-1,cmd=-1,d0=-1;
    if (board==&board_k518) { clk=4;cmd=3;d0=5; }
    else if (board==&board_k718) { clk=39;cmd=38;d0=40; }
    else { Serial.println("[Pokemon] TPK2 Idle: unsupported board");return false; }
    if (!SD_MMC.setPins(clk,cmd,d0) || !SD_MMC.begin("/sdcard",true,false)) {
        Serial.println("[Pokemon] TPK2 Idle: SD mount failed");
        return false;
    }
    bool success=false;
    if (SD_MMC.cardType()!=CARD_NONE) {
        File file=SD_MMC.open("/mons/p778.bin",FILE_READ);
        if (file && !file.isDirectory()) success=read_frames(file);
        if (file) file.close();
    }
    SD_MMC.end();
    if (!success) {
        pk_tpk2_idle_release();
        Serial.println("[Pokemon] TPK2 Idle: unavailable/invalid (read-only)");
        return false;
    }
    loaded=true;
    Serial.printf("[Pokemon] TPK2 Idle: ready, %u real frames (read-only)\n",frame_count);
    return true;
}
extern "C" const lv_img_dsc_t *pk_tpk2_idle_frame(unsigned i) {
    return loaded && i<frame_count ? &frames[i].image : nullptr;
}
extern "C" unsigned pk_tpk2_idle_frame_count(void) { return frame_count; }
extern "C" uint16_t pk_tpk2_idle_duration_ms(unsigned i) {
    return loaded && i<frame_count ? frames[i].ms : 200;
}
