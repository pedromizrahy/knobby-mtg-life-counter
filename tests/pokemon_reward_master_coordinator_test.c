#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_reward_master_coordinator.h"
typedef struct {
    pk_profile_t profile;
    int save_fail,prepared_fail,commit_fail,intent_prepare_fail,intent_finalize_fail;
    int event_order,prepare_order,save_order,commit_order,finalize_order;
    uint32_t game_id, pending_id,intent_game;
    pk_ledger_state_t state;
    pk_mt_intent_status_t intent;
    int spent, unlocks;
} mock_t;
static pk_ledger_state_t get(void *v,uint32_t u,uint32_t g) {
    mock_t *m=v;(void)u;
    return m->game_id==g?m->state:PK_LEDGER_ABSENT;
}
static bool record(void *v,uint32_t u,uint32_t g,pk_ledger_state_t s){
    mock_t *m=v;(void)u;
    if((s==PK_LEDGER_PREPARED&&m->prepared_fail) ||
       (s==PK_LEDGER_COMMITTED&&m->commit_fail))return false;
    m->game_id=g;m->state=s;
    m->pending_id=s==PK_LEDGER_PREPARED?g:0;
    if(s==PK_LEDGER_COMMITTED)m->commit_order=++m->event_order;
    return true;
}
static bool load(void *v,uint32_t u,pk_profile_t *p){
    mock_t *m=v;(void)u;*p=m->profile;return true;
}
static bool save(void *v,uint32_t u,const pk_profile_t *p){
    mock_t *m=v;(void)u;
    if(m->save_fail)return false;
    m->save_order=++m->event_order;m->profile=*p;return true;
}
static bool pending(void *v,uint32_t u,uint32_t *id){
    mock_t *m=v;(void)u;*id=m->pending_id;return true;
}
static pk_mt_intent_status_t intent_status(void *v,uint32_t u,uint32_t *g){
    mock_t *m=v;(void)u;*g=m->intent_game;return m->intent;
}
static bool intent_prepare(void *v,uint32_t u,uint32_t g,
                           const pk_profile_t *before,const pk_profile_t *after){
    mock_t *m=v;(void)u;
    assert(pk_master_trainer_first_level_100(before,after));
    if(m->intent_prepare_fail)return false;
    m->prepare_order=++m->event_order;
    m->intent_game=g;m->intent=PK_MT_INTENT_VALID;return true;
}
static bool intent_finalize(void *v,uint32_t u,uint32_t g){
    mock_t *m=v;(void)u;
    if(m->intent_finalize_fail || m->intent!=PK_MT_INTENT_VALID ||
       m->intent_game!=g || m->state!=PK_LEDGER_COMMITTED ||
       !m->commit_order || !m->save_order)
        return false;
    m->finalize_order=++m->event_order;
    if(!m->unlocks){m->unlocks++;m->spent=0;}
    return true;
}
static pk_reward_master_io_t io_for(mock_t *m){
    pk_reward_master_io_t io={
       {m,get,record,load,save,pending},
       intent_status,intent_prepare,intent_finalize
    };
    return io;
}
static mock_t init(uint8_t level){
    mock_t m={0};
    pk_profile_init(&m.profile);
    assert(pk_team_add(&m.profile,778,false));
    m.profile.team[0].level=level;
    m.intent=PK_MT_INTENT_MISSING;
    return m;
}
int main(void){
    pk_reward_t reward={};
    mock_t m=init(99);pk_reward_master_io_t io=io_for(&m);
    assert(pk_reward_master_transaction(&io,42,101,true,&reward)==PK_TX_APPLIED);
    assert(m.prepare_order && m.prepare_order<m.save_order);
    assert(m.save_order<m.commit_order && m.commit_order<m.finalize_order);
    assert(m.profile.team[0].level==100 && m.unlocks==1);
    assert(pk_reward_master_transaction(&io,42,101,true,&reward)==PK_TX_ALREADY);
    assert(reward.xp_awarded==0 && m.unlocks==1);
    m.spent=1;
    assert(pk_reward_master_transaction(&io,42,102,true,&reward)==PK_TX_APPLIED);
    assert(m.unlocks==1 && m.spent==1);
    assert(pk_reward_master_transaction(&io,42,102,true,&reward)==PK_TX_ALREADY);

    m=init(99);io=io_for(&m);m.intent_prepare_fail=1;
    assert(pk_reward_master_transaction(&io,42,201,true,&reward)==PK_TX_ERROR);
    assert(m.profile.team[0].level==99 && m.state==PK_LEDGER_ABSENT);

    /* Prepared achievement, but game journal PREPARE failed. Fail closed. */
    m=init(99);io=io_for(&m);m.prepared_fail=1;
    assert(pk_reward_master_transaction(&io,42,202,true,&reward)==PK_TX_ERROR);
    assert(m.intent==PK_MT_INTENT_VALID && m.profile.team[0].level==99);
    m.prepared_fail=0;
    assert(pk_reward_master_transaction(&io,42,202,true,&reward)==PK_TX_BLOCKED);

    /* Crash after profile save, before commit, then recover exactly once. */
    m=init(99);io=io_for(&m);m.commit_fail=1;
    assert(pk_reward_master_transaction(&io,42,203,true,&reward)==PK_TX_ERROR);
    assert(m.profile.team[0].level==100 && m.unlocks==0);
    assert(pk_reward_master_transaction(&io,42,204,true,&reward)==PK_TX_BLOCKED);
    m.commit_fail=0;
    assert(pk_reward_master_transaction(&io,42,203,true,&reward)==PK_TX_RECOVERED);
    assert(m.unlocks==1);
    assert(pk_reward_master_transaction(&io,42,203,true,&reward)==PK_TX_ALREADY);

    /* Failure while finalizing is recoverable after COMMITTED and profile save. */
    m=init(99);io=io_for(&m);m.intent_finalize_fail=1;
    assert(pk_reward_master_transaction(&io,42,205,true,&reward)==PK_TX_BLOCKED);
    assert(m.state==PK_LEDGER_COMMITTED && m.unlocks==0);
    assert(pk_reward_master_transaction(&io,42,206,true,&reward)==PK_TX_BLOCKED);
    m.intent_finalize_fail=0;
    assert(pk_reward_master_transaction(&io,42,205,true,&reward)==PK_TX_ALREADY);
    assert(m.unlocks==1);
    assert(pk_reward_master_transaction(&io,42,206,true,&reward)==PK_TX_APPLIED);
    assert(m.unlocks==1);

    /* Missing profile save on PREPARED must never re-apply XP. */
    m=init(99);io=io_for(&m);m.save_fail=1;
    assert(pk_reward_master_transaction(&io,42,207,true,&reward)==PK_TX_ERROR);
    m.save_fail=0;
    assert(pk_reward_master_transaction(&io,42,207,true,&reward)==PK_TX_BLOCKED);
    assert(m.profile.team[0].level==99 && m.unlocks==0);

    /* Ordinary games do not create a level-100 proof. */
    m=init(1);io=io_for(&m);
    assert(pk_reward_master_transaction(&io,42,208,false,&reward)==PK_TX_APPLIED);
    assert(m.intent==PK_MT_INTENT_MISSING && m.unlocks==0);

    /* Corrupt stored intent must not mutate any player state. */
    m=init(99);io=io_for(&m);m.intent=PK_MT_INTENT_CONFLICT;
    assert(pk_reward_master_transaction(&io,42,209,true,&reward)==PK_TX_BLOCKED);
    assert(m.profile.team[0].level==99);

    puts("Master Trainer transactional coordinator tests passed");
    return 0;
}
