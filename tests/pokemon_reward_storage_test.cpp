#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../knobby/src/pokemon_reward_storage.h"
#include "../knobby/src/pokemon_profile_store.h"
#include "../knobby/src/pokemon_reward_ledger.h"

static pk_profile_t saved;
static uint32_t journal_user=0, journal_session=0;
static pk_ledger_state_t journal_state=PK_LEDGER_ABSENT;
static bool present=false;
static bool fail_commit=false;

extern "C" bool pk_store_load(uint32_t user,pk_profile_t *out)
{
    if(!present || user!=42 || !out) return false;
    *out=saved;return true;
}
extern "C" bool pk_store_save(uint32_t user,const pk_profile_t *p)
{
    if(user!=42 || !p) return false;
    saved=*p;return true;
}
extern "C" pk_ledger_state_t pk_ledger_get(uint32_t user,uint32_t id)
{
    if(user==journal_user && id==journal_session) return journal_state;
    return PK_LEDGER_ABSENT;
}
extern "C" bool pk_ledger_record(uint32_t user,uint32_t id,pk_ledger_state_t state)
{
    if(state==PK_LEDGER_COMMITTED && fail_commit)return false;
    journal_user=user;journal_session=id;journal_state=state;
    return true;
}
extern "C" bool pk_ledger_find_pending(uint32_t user,uint32_t *out)
{
    if(!out)return false;
    *out=(user==journal_user && journal_state==PK_LEDGER_PREPARED)
        ?journal_session:0;
    return true;
}
int main()
{
    pk_reward_t reward={};
    assert(pk_reward_apply_existing(42,501,true,&reward)==PK_TX_NOT_READY);
    assert(journal_state==PK_LEDGER_ABSENT);
    present=true;
    pk_profile_init(&saved);
    assert(pk_team_add(&saved,778,false));
    assert(pk_reward_apply_existing(42,501,true,&reward)==PK_TX_APPLIED);
    assert(saved.wins==1 && saved.battles==1 && reward.xp_awarded==800);
    assert(pk_reward_apply_existing(42,501,true,&reward)==PK_TX_ALREADY);
    assert(saved.wins==1 && reward.xp_awarded==0);
    fail_commit=true;
    /* A new session's save may succeed before COMMITTED is recorded. */
    assert(pk_reward_apply_existing(42,502,false,&reward)==PK_TX_ERROR);
    assert(saved.battles==2 && journal_state==PK_LEDGER_PREPARED);
    assert(pk_reward_apply_existing(42,503,true,&reward)==PK_TX_BLOCKED);
    fail_commit=false;
    assert(pk_reward_apply_existing(42,502,false,&reward)==PK_TX_RECOVERED);
    assert(saved.battles==2);
    assert(pk_reward_apply_existing(42,502,false,&reward)==PK_TX_ALREADY);
    std::puts("Pokemon storage adapter tests passed");
    return 0;
}
