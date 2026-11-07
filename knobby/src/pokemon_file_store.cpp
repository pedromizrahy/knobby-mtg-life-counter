#include "pokemon_file_store.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* Separate file backend. No global hooks or NVS writes.
 * One slot is never modified while its peer is being updated.
 * The SPIFFS partition is shared with Playgroup queue and art, and the
 * backend never calls SPIFFS.begin(true), remove() or format().
 */
static const uint32_t MAGIC=0x32464B50UL; /* PKF2 */
static const uint32_t DIAGNOSTIC_ID=0xFFFFFFFDUL;
static const uint32_t REBOOT_DIAGNOSTIC_ID=0xFFFFFFFCUL;
static bool reserved_id(uint32_t id) {
    return id==DIAGNOSTIC_ID || id==REBOOT_DIAGNOSTIC_ID ||
           id==0xFFFFFFFEUL;
}
typedef struct {
    uint32_t magic;
    uint32_t user_id;
    uint32_t generation;
    pk_profile_t profile;
    uint32_t checksum;
} file_record_t;
static uint32_t hash_bytes(const uint8_t *p,size_t n)
{
    uint32_t h=2166136261UL;
    while(n--) { h^=*p++;h*=16777619UL; }
    return h;
}
static uint32_t record_checksum(const file_record_t *r)
{
    /* Hash named fields, not trailing structure padding. */
    const uint8_t *p=(const uint8_t*)r;
    size_t count=offsetof(file_record_t,checksum);
    return hash_bytes(p,count);
}
static void path_for(uint32_t id,unsigned slot,char path[24])
{
    snprintf(path,24,"/pkf_%08lx%c.bin",(unsigned long)id,slot?'b':'a');
}
static bool profile_valid(const pk_profile_t *p)
{
    if(p->version!=1 || p->team_count>PK_TEAM_MAX ||
       p->active_index >= (p->team_count?p->team_count:1) ||
       p->recent_next>=PK_RECENT_GAMES) return false;
    for(unsigned i=0;i<p->team_count;i++) {
        const pk_mon_t *m=&p->team[i];
        if(!m->species || m->species>PK_DEX_SPECIES ||
           !m->level || m->level>100 || m->shiny>1) return false;
    }
    return true;
}
static bool read_slot(uint32_t id,unsigned slot,file_record_t *out)
{
    char path[24];path_for(id,slot,path);
    File f=SPIFFS.open(path,FILE_READ);
    if(!f)return false;
    bool ok=f.size()==sizeof(*out) &&
        f.read((uint8_t*)out,sizeof(*out))==(int)sizeof(*out);
    f.close();
    return ok && out->magic==MAGIC && out->user_id==id &&
           profile_valid(&out->profile) &&
           out->checksum==record_checksum(out);
}
static pk_store_status_t inspect(uint32_t id,file_record_t *out,unsigned *slot)
{
    char a[24],b[24];path_for(id,0,a);path_for(id,1,b);
    bool ea=SPIFFS.exists(a),eb=SPIFFS.exists(b);
    if(!ea && !eb)return PK_STORE_MISSING;
    file_record_t ra={},rb={};
    bool va=ea && read_slot(id,0,&ra);
    bool vb=eb && read_slot(id,1,&rb);
    if(!va && !vb)return PK_STORE_CORRUPT;
    bool choose_b=vb && (!va || (int32_t)(rb.generation-ra.generation)>0);
    *out=choose_b?rb:ra;
    *slot=choose_b?1U:0U;
    return PK_STORE_VALID;
}
static bool write_slot(uint32_t id,unsigned slot,const file_record_t *rec)
{
    char path[24];path_for(id,slot,path);
    File f=SPIFFS.open(path,FILE_WRITE);
    if(!f)return false;
    size_t written=f.write((const uint8_t*)rec,sizeof(*rec));
    f.flush();
    f.close();
    if(written!=sizeof(*rec))return false;
    file_record_t check={};
    return read_slot(id,slot,&check) &&
           check.generation==rec->generation &&
           memcmp(&check.profile,&rec->profile,sizeof(rec->profile))==0;
}
extern "C" pk_store_status_t pk_file_inspect(uint32_t id,pk_profile_t *out)
{
    if(!id || reserved_id(id) || !SPIFFS.begin(false))
        return PK_STORE_ERROR;
    file_record_t record={};unsigned slot=0;
    pk_store_status_t status=inspect(id,&record,&slot);
    if(status==PK_STORE_VALID && out)*out=record.profile;
    return status;
}
extern "C" bool pk_file_create_starter(uint32_t id)
{
    if(!id || reserved_id(id) || !SPIFFS.begin(false))return false;
    file_record_t existing={};unsigned slot=0;
    if(inspect(id,&existing,&slot)!=PK_STORE_MISSING)return false;
    file_record_t record={};
    record.magic=MAGIC;record.user_id=id;record.generation=1;
    pk_profile_init(&record.profile);
    if(!pk_team_add(&record.profile,778,false))return false;
    record.checksum=record_checksum(&record);
    return write_slot(id,0,&record);
}
extern "C" bool pk_file_save(uint32_t id,const pk_profile_t *profile)
{
    if(!id || reserved_id(id) || !profile ||
       !profile_valid(profile) || !SPIFFS.begin(false))return false;
    file_record_t old={};unsigned slot=0;
    if(inspect(id,&old,&slot)!=PK_STORE_VALID)return false;
    file_record_t record={};
    record.magic=MAGIC;record.user_id=id;
    record.generation=old.generation+1U;
    record.profile=*profile;
    record.checksum=record_checksum(&record);
    return write_slot(id,1U-slot,&record);
}

/* Explicit import from an already-validated NVS profile.
 * Writes only a private staging path; never truncates either profile slot.
 * If power is lost with a staging file present, fail closed on retry:
 * manual diagnosis is safer than silently discarding possible data.
 */
extern "C" bool pk_file_import_if_missing(uint32_t id,const pk_profile_t *profile)
{
    if(!id || reserved_id(id) || !profile ||
       !profile_valid(profile) || !SPIFFS.begin(false)) return false;

    file_record_t existing={};
    unsigned slot=0;
    if(inspect(id,&existing,&slot)!=PK_STORE_MISSING) return false;

    char destination[24];
    path_for(id,0,destination);
    char staging[24];
    snprintf(staging,sizeof(staging),"/pkm_%08lx.tmp",(unsigned long)id);
    if(SPIFFS.exists(staging) || SPIFFS.exists(destination)) return false;

    file_record_t record={};
    record.magic=MAGIC;record.user_id=id;record.generation=1;
    record.profile=*profile;
    record.checksum=record_checksum(&record);

    File f=SPIFFS.open(staging,FILE_WRITE);
    if(!f) return false;
    const size_t written=f.write((const uint8_t*)&record,sizeof(record));
    f.flush();
    f.close();
    /* Never delete a partial staging file without a deliberate recovery
     * procedure: it records that a migration attempt was interrupted. */
    if(written!=sizeof(record)) return false;

    File verify=SPIFFS.open(staging,FILE_READ);
    if(!verify) return false;
    file_record_t copy={};
    bool valid=verify.size()==sizeof(copy) &&
               verify.read((uint8_t*)&copy,sizeof(copy))==(int)sizeof(copy);
    verify.close();
    if(!valid || copy.magic!=MAGIC || copy.user_id!=id ||
       copy.checksum!=record_checksum(&copy) ||
       memcmp(&copy.profile,profile,sizeof(*profile))!=0)
        return false;

    /* No other writer may operate on this user while migrating. */
    if(SPIFFS.exists(destination) || !SPIFFS.rename(staging,destination))
        return false;
    pk_profile_t recovered={};
    return pk_file_inspect(id,&recovered)==PK_STORE_VALID &&
           memcmp(&recovered,profile,sizeof(*profile))==0;
}

/* Deliberately uses a reserved ID, never a Playgroup user ID. Public
 * profile APIs reject this ID; only this explicit diagnostic uses it.
 * Refuses to touch pre-existing files even if created by an interrupted
 * diagnostic. Never modifies NVS or any non-diagnostic SPIFFS path. */
extern "C" bool pk_file_profile_selftest(void)
{
    const uint32_t id=DIAGNOSTIC_ID;
    if(!SPIFFS.begin(false))return false;
    char a[24],b[24];
    path_for(id,0,a);path_for(id,1,b);
    if(SPIFFS.exists(a) || SPIFFS.exists(b))return false;
    const size_t total=SPIFFS.totalBytes(),used=SPIFFS.usedBytes();
    if(!total || used>total || total-used<256U*1024U)return false;

    file_record_t first={},next={},loaded={};
    unsigned current=0;
    bool created_a=false,created_b=false,ok=false;
    pk_reward_t reward={};
    first.magic=MAGIC;first.user_id=id;first.generation=1;
    pk_profile_init(&first.profile);
    if(!pk_team_add(&first.profile,778,false))return false;
    first.checksum=record_checksum(&first);

    /* Mark the path as ours before writing; cleanup is limited to files
     * created during THIS invocation and only if they still exist. */
    created_a=true;
    if(!write_slot(id,0,&first))goto cleanup;
    if(inspect(id,&loaded,&current)!=PK_STORE_VALID || current!=0 ||
       memcmp(&loaded.profile,&first.profile,sizeof(pk_profile_t))!=0)
        goto cleanup;

    next=first;
    next.generation=2;
    if(!pk_award_game(&next.profile,0xD1A10001UL,true,&reward) ||
       reward.xp_awarded!=800 || reward.new_pokeballs!=4 ||
       next.profile.team[0].level<=1 ||
       next.profile.balls[PK_BALL_POKE]!=4)
        goto cleanup;
    next.checksum=record_checksum(&next);
    created_b=true;
    if(!write_slot(id,1,&next))goto cleanup;
    if(inspect(id,&loaded,&current)!=PK_STORE_VALID || current!=1 ||
       memcmp(&loaded.profile,&next.profile,sizeof(pk_profile_t))!=0)
        goto cleanup;

    /* Damage ONLY our own diagnostic B file to verify fallback to A.
     * Intentionally do not mutate any real profile or ledger file. */
    {
        File f=SPIFFS.open(b,FILE_WRITE);
        if(!f)goto cleanup;
        const uint8_t invalid=0;
        size_t n=f.write(&invalid,1);
        f.flush();
        f.close();
        if(n!=1)goto cleanup;
    }
    if(inspect(id,&loaded,&current)!=PK_STORE_VALID || current!=0 ||
       memcmp(&loaded.profile,&first.profile,sizeof(pk_profile_t))!=0)
        goto cleanup;

    /* Rebuild B with the updated snapshot and verify latest generation. */
    if(!write_slot(id,1,&next))goto cleanup;
    if(inspect(id,&loaded,&current)!=PK_STORE_VALID || current!=1 ||
       memcmp(&loaded.profile,&next.profile,sizeof(pk_profile_t))!=0)
        goto cleanup;
    ok=true;
cleanup:
    /* Never clean up pre-existing diagnostic files. A failed deletion
     * yields FAIL and the next invocation refuses to overwrite residue. */
    if(created_b && SPIFFS.exists(b) && !SPIFFS.remove(b))ok=false;
    if(created_a && SPIFFS.exists(a) && !SPIFFS.remove(a))ok=false;
    return ok;
}

/* Persistent reboot test using a second reserved ID, physically separate
 * from the same-session corruption test and every real Playgroup user.
 * No NVS writes. There is intentionally NO automatic boot-time action.
 * PREP must be explicitly followed by a reboot and then VERIFY.
 */
static bool reboot_expected(file_record_t *a,file_record_t *b)
{
    *a={};*b={};
    a->magic=MAGIC;
    a->user_id=REBOOT_DIAGNOSTIC_ID;
    a->generation=1;
    pk_profile_init(&a->profile);
    if(!pk_team_add(&a->profile,778,false))return false;
    a->checksum=record_checksum(a);
    *b=*a;
    b->generation=2;
    pk_reward_t award={};
    if(!pk_award_game(&b->profile,0xD1A10002UL,true,&award) ||
       award.xp_awarded!=800 || award.new_pokeballs!=4)return false;
    b->checksum=record_checksum(b);
    return true;
}
extern "C" bool pk_file_reboot_prep(void)
{
    if(!SPIFFS.begin(false))return false;
    const size_t total=SPIFFS.totalBytes(),used=SPIFFS.usedBytes();
    if(!total || used>total || total-used<256U*1024U)return false;
    char a[24],b[24];
    path_for(REBOOT_DIAGNOSTIC_ID,0,a);
    path_for(REBOOT_DIAGNOSTIC_ID,1,b);
    /* A prior interrupted PREP must never be reset or overwritten. */
    if(SPIFFS.exists(a) || SPIFFS.exists(b))return false;
    file_record_t initial={},updated={},loaded={};
    if(!reboot_expected(&initial,&updated))return false;
    /* On any write failure leave the diagnostic paths untouched for
     * inspection; no cleanup of partially written data is attempted. */
    if(!write_slot(REBOOT_DIAGNOSTIC_ID,0,&initial))return false;
    if(!write_slot(REBOOT_DIAGNOSTIC_ID,1,&updated))return false;
    unsigned selected=0;
    return read_slot(REBOOT_DIAGNOSTIC_ID,0,&loaded) &&
           loaded.generation==1 &&
           memcmp(&loaded.profile,&initial.profile,sizeof(pk_profile_t))==0 &&
           inspect(REBOOT_DIAGNOSTIC_ID,&loaded,&selected)==PK_STORE_VALID &&
           selected==1 &&
           memcmp(&loaded.profile,&updated.profile,sizeof(pk_profile_t))==0;
}
extern "C" bool pk_file_reboot_verify(void)
{
    if(!SPIFFS.begin(false))return false;
    char a[24],b[24];
    path_for(REBOOT_DIAGNOSTIC_ID,0,a);
    path_for(REBOOT_DIAGNOSTIC_ID,1,b);
    if(!SPIFFS.exists(a) || !SPIFFS.exists(b))return false;
    file_record_t initial={},updated={},loaded_a={},loaded_b={},latest={};
    if(!reboot_expected(&initial,&updated))return false;
    unsigned selected=0;
    /* Check the full serialized profile and generation in BOTH slots.
     * Unexpected bytes or corrupted data: FAIL without deleting either. */
    if(!read_slot(REBOOT_DIAGNOSTIC_ID,0,&loaded_a) ||
       !read_slot(REBOOT_DIAGNOSTIC_ID,1,&loaded_b) ||
       loaded_a.generation!=initial.generation ||
       loaded_b.generation!=updated.generation ||
       memcmp(&loaded_a.profile,&initial.profile,sizeof(pk_profile_t))!=0 ||
       memcmp(&loaded_b.profile,&updated.profile,sizeof(pk_profile_t))!=0 ||
       inspect(REBOOT_DIAGNOSTIC_ID,&latest,&selected)!=PK_STORE_VALID ||
       selected!=1)return false;
    /* After successful readback remove ONLY the two reserved files.
     * A failed deletion yields FAIL; further PREP refuses existing residue. */
    bool clean_b=SPIFFS.remove(b);
    bool clean_a=SPIFFS.remove(a);
    return clean_a && clean_b;
}
