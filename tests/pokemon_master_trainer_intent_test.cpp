#include <cassert>
#include <cstdio>
#include "SPIFFS.h"
#include "../knobby/src/pokemon_master_trainer_intent.h"
#include "../knobby/src/pokemon_master_trainer_store.h"
MockSPIFFS SPIFFS;
struct Context {
  pk_profile_t durable{};
  bool loaded=false;
  pk_ledger_state_t state=PK_LEDGER_ABSENT;
};
static pk_ledger_state_t ledger(void *v,uint32_t id,uint32_t g) {
  (void)id;(void)g;return ((Context *)v)->state;
}
static bool load(void *v,uint32_t id,pk_profile_t *out) {
  (void)id;
  Context *c=(Context *)v;
  if(!c->loaded)return false;
  *out=c->durable;return true;
}
static void crossing(pk_profile_t *b,pk_profile_t *a,uint32_t game) {
  pk_profile_init(b);assert(pk_team_add(b,778,false));
  b->team[0].level=99;*a=*b;
  pk_reward_t r={};assert(pk_award_game(a,game,true,&r));
  assert(a->team[0].level==100);
}
int main() {
  pk_profile_t b={},a={};
  crossing(&b,&a,71);
  Context context{};
  pk_mt_recovery_io_t io={&context,ledger,load};
  uint32_t session=0;
  assert(pk_mt_intent_status(42,&session)==PK_MT_INTENT_MISSING);
  assert(!pk_mt_intent_prepare(42,72,&b,&a));
  assert(pk_mt_intent_prepare(42,71,&b,&a));
  assert(pk_mt_intent_status(42,&session)==PK_MT_INTENT_VALID && session==71);
  assert(!pk_mt_intent_prepare(42,71,&b,&a));
  assert(!pk_mt_intent_finalize(42,71,&io)); /* not committed */
  context.loaded=true;context.durable=b;
  context.state=PK_LEDGER_COMMITTED;
  assert(!pk_mt_intent_finalize(42,71,&io)); /* profile not persisted */
  context.durable=a;
  assert(pk_mt_intent_finalize(42,71,&io));
  pk_mt_state_t badge={};
  assert(pk_mt_inspect(42,&badge)==PK_MT_VALID && badge.unlocked && !badge.spent);
  assert(pk_mt_intent_finalize(42,71,&io)); /* retry after reboot */
  assert(!pk_mt_intent_finalize(42,72,&io));
  assert(pk_mt_mark_spent(42));
  assert(pk_mt_intent_finalize(42,71,&io)); /* cannot resurrect spent ball */
  assert(pk_mt_inspect(42,&badge)==PK_MT_VALID && badge.spent);

  /* Crash before journal commit: no claim even if profile was saved. */
  crossing(&b,&a,72);
  assert(pk_mt_intent_prepare(43,72,&b,&a));
  context.durable=a;context.state=PK_LEDGER_PREPARED;
  assert(!pk_mt_intent_finalize(43,72,&io));
  context.state=PK_LEDGER_COMMITTED;
  assert(pk_mt_intent_finalize(43,72,&io));
  assert(!pk_mt_intent_prepare(43,72,&b,&a));

  /* Torn immutable proof must block the unlock (no reset/overwrite). */
  crossing(&b,&a,73);
  SPIFFS.fail_write=true;
  assert(!pk_mt_intent_prepare(44,73,&b,&a));
  SPIFFS.fail_write=false;
  assert(pk_mt_intent_status(44,nullptr)==PK_MT_INTENT_CONFLICT);
  assert(!pk_mt_intent_prepare(44,73,&b,&a));
  context.durable=a;
  assert(!pk_mt_intent_finalize(44,73,&io));

  /* Corrupt wallet blocks the intent and never issues a duplicate. */
  crossing(&b,&a,74);
  assert(pk_mt_intent_prepare(45,74,&b,&a));
  context.durable=a;
  SPIFFS.fail_write=true;
  assert(!pk_mt_intent_finalize(45,74,&io));
  SPIFFS.fail_write=false;
  assert(!pk_mt_intent_finalize(45,74,&io));
  assert(pk_mt_inspect(45,&badge)==PK_MT_CORRUPT);

  SPIFFS.mount_ok=false;
  assert(!pk_mt_intent_finalize(43,72,&io));
  SPIFFS.mount_ok=true;
  std::puts("Master Trainer write-ahead recovery tests passed");
}
