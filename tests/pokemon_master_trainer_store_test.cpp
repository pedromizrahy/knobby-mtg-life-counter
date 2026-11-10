#include <cassert>
#include <cstdio>
#include "SPIFFS.h"
#include "../knobby/src/pokemon_master_trainer_store.h"

MockSPIFFS SPIFFS;

static void crossing(pk_profile_t *before,pk_profile_t *after,
                     uint32_t game_id)
{
    pk_profile_init(before);
    assert(pk_team_add(before,778,false));
    before->team[0].level=99;
    *after=*before;
    pk_reward_t r={};
    assert(pk_award_game(after,game_id,true,&r));
    assert(after->team[0].level==100);
}

int main()
{
    pk_profile_t before={},after={};
    pk_mt_state_t s={};
    crossing(&before,&after,501);
    assert(pk_mt_inspect(42,&s)==PK_MT_MISSING);
    assert(!pk_mt_unlock(0,501,&before,&after));
    assert(!pk_mt_unlock(42,502,&before,&after));
    assert(!pk_mt_unlock(42,501,&after,&before));
    assert(pk_mt_unlock(42,501,&before,&after));
    assert(pk_mt_inspect(42,&s)==PK_MT_VALID);
    assert(s.unlocked && !s.spent && s.unlock_game_id==501);
    assert(!pk_mt_unlock(42,501,&before,&after));
    assert(pk_mt_mark_spent(42));
    assert(pk_mt_inspect(42,&s)==PK_MT_VALID && s.spent);
    assert(!pk_mt_mark_spent(42));
    /* Restart: calling inspect again reads the durable record. */
    assert(pk_mt_inspect(42,&s)==PK_MT_VALID && s.spent);

    /* A torn/corrupt newest record must NOT resurrect spent currency
     * by falling back to the still-valid previous unspent slot. */
    auto damaged=SPIFFS.files;
    SPIFFS.files["/pmt_0000002ab.bin"][0]^=0x7e;
    assert(pk_mt_inspect(42,&s)==PK_MT_CORRUPT);
    assert(!pk_mt_mark_spent(42));
    assert(!pk_mt_unlock(42,501,&before,&after));
    SPIFFS.files=damaged;
    assert(pk_mt_inspect(42,&s)==PK_MT_VALID && s.spent);

    /* Empty profile cannot unlock; independent second user can. */
    pk_profile_t no_cross=before;
    assert(!pk_mt_unlock(99,503,&no_cross,&no_cross));
    crossing(&before,&after,504);
    assert(pk_mt_unlock(99,504,&before,&after));
    assert(pk_mt_inspect(99,&s)==PK_MT_VALID && !s.spent);
    assert(pk_mt_inspect(42,&s)==PK_MT_VALID && s.spent);

    /* An interrupted first write leaves a file and blocks duplicate claims. */
    crossing(&before,&after,600);
    SPIFFS.fail_write=true;
    assert(!pk_mt_unlock(80,600,&before,&after));
    SPIFFS.fail_write=false;
    assert(pk_mt_inspect(80,&s)==PK_MT_CORRUPT);
    assert(!pk_mt_unlock(80,600,&before,&after));

    /* A failed spend leaves the entitlement unspent OR blocks access;
     * it must never claim that a spend succeeded. */
    crossing(&before,&after,700);
    assert(pk_mt_unlock(81,700,&before,&after));
    SPIFFS.fail_write=true;
    assert(!pk_mt_mark_spent(81));
    SPIFFS.fail_write=false;
    assert(pk_mt_inspect(81,&s)==PK_MT_CORRUPT);
    assert(!pk_mt_mark_spent(81));

    SPIFFS.mount_ok=false;
    assert(pk_mt_inspect(42,&s)==PK_MT_ERROR);
    assert(!pk_mt_unlock(82,700,&before,&after));
    SPIFFS.mount_ok=true;
    std::puts("Master Trainer durable wallet tests passed");
}
