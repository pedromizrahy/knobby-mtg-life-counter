#include "pokemon_reward_ledger.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <stdio.h>

/* Append-only per-user ledger. It uses the existing SPIFFS partition,
 * never formats it and never touches the 20 KiB NVS partition.
 * Records are deliberately fixed-width with no padding or native-endian
 * structs serialized to flash. A partial/corrupt record blocks writes.
 */
static const uint32_t PK_LEDGER_MAGIC = 0x314C4B50UL; /* PKL1 */
static const size_t PK_LEDGER_RECORD_BYTES = 16U;

static void put32(uint8_t *b, uint32_t v)
{
    b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8);
    b[2]=(uint8_t)(v>>16); b[3]=(uint8_t)(v>>24);
}
static uint32_t get32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) |
           ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}
static uint32_t hash12(const uint8_t *b)
{
    uint32_t h=2166136261UL;
    for (unsigned i=0;i<12U;i++) { h^=b[i]; h*=16777619UL; }
    return h;
}
static void path_for(uint32_t user_id, char *path, size_t n)
{
    snprintf(path,n,"/pkl_%08lx.log",(unsigned long)user_id);
}
static bool decode(const uint8_t *b, uint32_t *session,
                   pk_ledger_state_t *state)
{
    if(get32(b)!=PK_LEDGER_MAGIC || get32(b+12)!=hash12(b))
        return false;
    uint32_t value=get32(b+8);
    if(value!=PK_LEDGER_PREPARED && value!=PK_LEDGER_COMMITTED)
        return false;
    *session=get32(b+4);
    if(!*session) return false;
    *state=(pk_ledger_state_t)value;
    return true;
}

/* A scan also validates ALL entries, not only those for the requested ID.
 * We cannot trust an incomplete ledger or overwrite a damaged tail.
 */
static pk_ledger_state_t scan(uint32_t user_id, uint32_t session_id)
{
    char path[24];
    path_for(user_id,path,sizeof(path));
    if(!SPIFFS.exists(path)) return PK_LEDGER_ABSENT;
    File file=SPIFFS.open(path,FILE_READ);
    if(!file) return PK_LEDGER_ERROR;
    if(file.size()%PK_LEDGER_RECORD_BYTES) {
        file.close();
        return PK_LEDGER_ERROR;
    }
    pk_ledger_state_t result=PK_LEDGER_ABSENT;
    uint8_t entry[PK_LEDGER_RECORD_BYTES];
    while(file.available()) {
        if(file.read(entry,sizeof(entry))!=(int)sizeof(entry)) {
            file.close();
            return PK_LEDGER_ERROR;
        }
        uint32_t id=0;
        pk_ledger_state_t state=PK_LEDGER_ERROR;
        if(!decode(entry,&id,&state)) {
            file.close();
            return PK_LEDGER_ERROR;
        }
        if(id==session_id) {
            /* Once committed, a session can never revert to prepared. */
            if(result==PK_LEDGER_COMMITTED && state!=PK_LEDGER_COMMITTED) {
                file.close();
                return PK_LEDGER_ERROR;
            }
            result=state;
        }
    }
    file.close();
    return result;
}
extern "C" pk_ledger_state_t pk_ledger_get(uint32_t user_id,uint32_t session_id)
{
    if(!user_id || !session_id || !SPIFFS.begin(false))
        return PK_LEDGER_ERROR;
    return scan(user_id,session_id);
}
extern "C" bool pk_ledger_record(uint32_t user_id,uint32_t session_id,
                                pk_ledger_state_t new_state)
{
    if(!user_id || !session_id ||
       (new_state!=PK_LEDGER_PREPARED && new_state!=PK_LEDGER_COMMITTED) ||
       !SPIFFS.begin(false)) return false;

    pk_ledger_state_t old=scan(user_id,session_id);
    if(old==PK_LEDGER_ERROR) return false;
    if(old==new_state) return true;
    if(new_state==PK_LEDGER_PREPARED && old!=PK_LEDGER_ABSENT)
        return false;
    if(new_state==PK_LEDGER_COMMITTED && old!=PK_LEDGER_PREPARED)
        return false;

    char path[24];
    path_for(user_id,path,sizeof(path));
    uint8_t entry[PK_LEDGER_RECORD_BYTES];
    put32(entry,PK_LEDGER_MAGIC);
    put32(entry+4,session_id);
    put32(entry+8,(uint32_t)new_state);
    put32(entry+12,hash12(entry));
    File file=SPIFFS.open(path,FILE_APPEND);
    if(!file) return false;
    bool ok=file.write(entry,sizeof(entry))==sizeof(entry);
    file.flush();
    file.close();
    /* Verify durable read-back; ambiguous writes fail closed on next read. */
    return ok && scan(user_id,session_id)==new_state;
}

/* Conservative pending scan: validate the complete file first; for each
 * PREPARED record check its latest state. It allocates no heap memory.
 * The two-pass scan is deliberately isolated from the main Magic loop
 * until transactional reward delivery is enabled and performance tested. */
extern "C" bool pk_ledger_find_pending(uint32_t user_id,uint32_t *pending_id)
{
    if(!user_id || !pending_id || !SPIFFS.begin(false)) return false;
    *pending_id=0;
    if(scan(user_id,0)==PK_LEDGER_ERROR) return false;
    char path[24];
    path_for(user_id,path,sizeof(path));
    if(!SPIFFS.exists(path)) return true;
    File file=SPIFFS.open(path,FILE_READ);
    if(!file) return false;
    uint8_t entry[PK_LEDGER_RECORD_BYTES];
    while(file.available()) {
        if(file.read(entry,sizeof(entry))!=(int)sizeof(entry)) {
            file.close();return false;
        }
        uint32_t session=0;
        pk_ledger_state_t state=PK_LEDGER_ERROR;
        if(!decode(entry,&session,&state)) {
            file.close();return false;
        }
        if(state==PK_LEDGER_PREPARED) {
            pk_ledger_state_t current=scan(user_id,session);
            if(current==PK_LEDGER_ERROR) { file.close();return false; }
            if(current==PK_LEDGER_PREPARED) {
                *pending_id=session;
                file.close();return true;
            }
        }
    }
    file.close();return true;
}
