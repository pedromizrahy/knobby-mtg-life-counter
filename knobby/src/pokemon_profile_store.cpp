#include "pokemon_profile_store.h"
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

/* Dedicated NVS namespace, two alternating records per player.
 * Read checks checksum and always selects the latest valid generation.
 * A partially interrupted write leaves the previous valid slot intact.
 */
static const char *PK_NAMESPACE = "pk_companions";
static const uint32_t PK_STORE_MAGIC = 0x504B4331UL;
typedef struct {
    uint32_t magic;
    uint32_t user_id;
    uint32_t generation;
    pk_profile_t profile;
    uint32_t checksum;
} pk_record_t;

static uint32_t checksum_bytes(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t h = 2166136261UL;
    while (length--) { h ^= *bytes++; h *= 16777619UL; }
    return h;
}
static void make_key(uint32_t id, unsigned slot, char key[15])
{
    snprintf(key, 15, "p%08lx%c", (unsigned long)id, slot ? 'b' : 'a');
}
static bool valid_profile(const pk_profile_t *p)
{
    if (!p || p->version != 1 || p->team_count > PK_TEAM_MAX ||
        p->active_index >= (p->team_count ? p->team_count : 1) ||
        p->recent_next >= PK_RECENT_GAMES) return false;
    for (unsigned i=0; i<p->team_count; i++) {
        if (p->team[i].species == 0 || p->team[i].species > PK_DEX_SPECIES ||
            p->team[i].level == 0 || p->team[i].level > 100 ||
            p->team[i].shiny > 1) return false;
    }
    return true;
}
static bool load_slot(Preferences &prefs, uint32_t id,
                      unsigned slot, pk_record_t *r)
{
    char key[15];
    make_key(id,slot,key);
    if (prefs.getBytesLength(key) != sizeof(*r) ||
        prefs.getBytes(key,r,sizeof(*r)) != sizeof(*r)) return false;
    return r->magic == PK_STORE_MAGIC && r->user_id == id &&
           valid_profile(&r->profile) &&
           r->checksum == checksum_bytes(r,sizeof(*r)-sizeof(r->checksum));
}
static bool latest(Preferences &prefs,uint32_t id,pk_record_t *out,
                   unsigned *slot)
{
    pk_record_t a,b;
    bool av=load_slot(prefs,id,0,&a);
    bool bv=load_slot(prefs,id,1,&b);
    if (!av && !bv) return false;
    /* Wrap-safe generation comparison. */
    if (bv && (!av || (int32_t)(b.generation-a.generation)>0)) {
        *out=b;*slot=1;
    } else { *out=a;*slot=0; }
    return true;
}
extern "C" bool pk_store_load(uint32_t id, pk_profile_t *out)
{
    if (!id || !out) return false;
    Preferences prefs;
    if (!prefs.begin(PK_NAMESPACE,true)) return false;
    pk_record_t r;
    unsigned slot=0;
    bool ok=latest(prefs,id,&r,&slot);
    prefs.end();
    if(ok) *out=r.profile;
    return ok;
}
extern "C" bool pk_store_save(uint32_t id,const pk_profile_t *p)
{
    if (!id || !valid_profile(p)) return false;
    Preferences prefs;
    if (!prefs.begin(PK_NAMESPACE,false)) return false;
    pk_record_t last;
    unsigned slot=1;
    bool exists=latest(prefs,id,&last,&slot);
    unsigned destination=exists?(1U-slot):0U;
    pk_record_t r={};
    r.magic=PK_STORE_MAGIC;
    r.user_id=id;
    r.generation=exists?last.generation+1U:1U;
    r.profile=*p;
    r.checksum=checksum_bytes(&r,sizeof(r)-sizeof(r.checksum));
    char key[15];
    make_key(id,destination,key);
    bool ok=prefs.putBytes(key,&r,sizeof(r))==sizeof(r);
    /* A reported write is not enough: verify the new slot immediately.
       Do not alter the previous good slot on failure. */
    if(ok) {
        pk_record_t check;
        ok=load_slot(prefs,id,destination,&check) &&
           check.generation==r.generation &&
           memcmp(&check.profile,&r.profile,sizeof(pk_profile_t))==0;
    }
    prefs.end();
    return ok;
}

/* A diagnostic the operator may invoke explicitly in a later test build.
 * No automatic reads/writes, no use of the Magic 'knobby' namespace.
 * Reserved ID and cleanup ensure genuine player records are untouched.
 */
extern "C" bool pk_store_selftest(void)
{
    const uint32_t diagnostic_id = 0xFFFFFFFEUL;
    pk_profile_t original,updated,loaded;
    pk_reward_t reward;
    bool ok=false;
    Preferences prefs;
    char a[15],b[15];
    make_key(diagnostic_id,0,a);
    make_key(diagnostic_id,1,b);
    if (!prefs.begin(PK_NAMESPACE,false)) return false;
    /* Refuse to overwrite any pre-existing diagnostic data. */
    if (prefs.isKey(a) || prefs.isKey(b)) { prefs.end(); return false; }
    prefs.end();

    pk_profile_init(&original);
    if (!pk_team_add(&original,778,false)) return false;
    if (!pk_store_save(diagnostic_id,&original)) goto cleanup;
    if (!pk_store_load(diagnostic_id,&loaded) ||
        memcmp(&loaded,&original,sizeof(original))!=0) goto cleanup;
    updated=original;
    if (!pk_award_game(&updated,12345U,true,&reward)) goto cleanup;
    if (!pk_store_save(diagnostic_id,&updated)) goto cleanup;
    if (!pk_store_load(diagnostic_id,&loaded) ||
        memcmp(&loaded,&updated,sizeof(updated))!=0) goto cleanup;
    ok=true;
cleanup:
    if (prefs.begin(PK_NAMESPACE,false)) {
        prefs.remove(a);
        prefs.remove(b);
        prefs.end();
    }
    return ok;
}
