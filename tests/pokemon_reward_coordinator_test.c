#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_reward_coordinator.h"
typedef struct {
  pk_profile_t p;
  int has_profile,fail_save,fail_prepare,fail_commit,fail_read;
  uint32_t active_id, pending_id;
  pk_ledger_state_t state;
} mock_t;
static pk_ledger_state_t get(void *v,uint32_t u,uint32_t g) {
  mock_t *m=v; (void)u;
  if(m->fail_read)return PK_LEDGER_ERROR;
  return m->active_id==g?m->state:PK_LEDGER_ABSENT;
}
static bool record(void *v,uint32_t u,uint32_t g,pk_ledger_state_t s) {
  mock_t *m=v; (void)u;
  if((s==PK_LEDGER_PREPARED&&m->fail_prepare)||
     (s==PK_LEDGER_COMMITTED&&m->fail_commit)) return false;
  m->active_id=g;m->state=s;
  m->pending_id=(s==PK_LEDGER_PREPARED)?g:0;
  return true;
}
static bool load(void *v,uint32_t u,pk_profile_t *out){
  mock_t *m=v;(void)u;
  if(!m->has_profile)return false;
  *out=m->p;return true;
}
static bool save(void *v,uint32_t u,const pk_profile_t *p){
  mock_t *m=v;(void)u;
  if(m->fail_save)return false;
  m->p=*p;return true;
}
static bool pending(void *v,uint32_t u,uint32_t *out){
  mock_t *m=v;(void)u;
  if(m->fail_read)return false;
  *out=m->pending_id;return true;
}
static pk_reward_io_t io_for(mock_t *m) {
  pk_reward_io_t io={m,get,record,load,save,pending};
  return io;
}
static mock_t init(void) {
  mock_t m={0};
  pk_profile_init(&m.p);
  assert(pk_team_add(&m.p,778,false));
  m.has_profile=1;
  return m;
}
int main(void){
  pk_reward_t reward;
  mock_t m=init();
  pk_reward_io_t io=io_for(&m);
  assert(pk_reward_transaction(&io,22,101,true,&reward)==PK_TX_APPLIED);
  assert(reward.xp_awarded==800 && reward.new_pokeballs==1);
  assert(m.p.wins==1 && m.p.battles==1);
  assert(pk_reward_transaction(&io,22,101,true,&reward)==PK_TX_ALREADY);
  assert(m.p.wins==1 && reward.xp_awarded==0);

  /* Simulated power loss after profile save, before ledger commit. */
  m=init();io=io_for(&m);m.fail_commit=1;
  assert(pk_reward_transaction(&io,22,102,true,&reward)==PK_TX_ERROR);
  assert(m.p.wins==1 && m.state==PK_LEDGER_PREPARED);
  assert(pk_reward_transaction(&io,22,103,true,&reward)==PK_TX_BLOCKED);
  m.fail_commit=0;
  assert(pk_reward_transaction(&io,22,102,true,&reward)==PK_TX_RECOVERED);
  assert(reward.xp_awarded==0 && m.p.wins==1);
  assert(pk_reward_transaction(&io,22,102,true,&reward)==PK_TX_ALREADY);

  /* Crash before profile persistence is ambiguous: never reapply. */
  m=init();io=io_for(&m);m.fail_save=1;
  assert(pk_reward_transaction(&io,22,104,true,&reward)==PK_TX_ERROR);
  m.fail_save=0;
  assert(pk_reward_transaction(&io,22,104,true,&reward)==PK_TX_BLOCKED);
  assert(m.p.battles==0);

  /* Failed PREPARED write must not update profile. */
  m=init();io=io_for(&m);m.fail_prepare=1;
  assert(pk_reward_transaction(&io,22,105,true,&reward)==PK_TX_ERROR);
  assert(m.p.battles==0);

  /* Missing profile, corrupt ledger, and preexisting profile ID. */
  m=init();io=io_for(&m);m.has_profile=0;
  assert(pk_reward_transaction(&io,22,106,true,&reward)==PK_TX_NOT_READY);
  m.has_profile=1;m.fail_read=1;
  assert(pk_reward_transaction(&io,22,106,true,&reward)==PK_TX_ERROR);
  m.fail_read=0;
  assert(pk_award_game(&m.p,107,true,&reward));
  assert(pk_reward_transaction(&io,22,107,true,&reward)==PK_TX_BLOCKED);
  puts("Pokemon reward crash-recovery coordinator tests passed");
  return 0;
}
