#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_evolution_transaction.h"
typedef struct {
    pk_profile_t profile;
    pk_evolve_tx_intent_t intent;
    pk_evolve_intent_status_t status;
    int fail_create,fail_save,fail_complete;
    unsigned profile_writes;
} Test;
static pk_evolve_intent_status_t get(void *ctx,uint32_t id,pk_evolve_tx_intent_t *o){
    Test *t=(Test*)ctx;
    if(t->status>0 && id==t->intent.user_id)*o=t->intent;
    return t->status;
}
static bool create(void *ctx,const pk_evolve_tx_intent_t *i){
    Test *t=(Test*)ctx;
    if(t->status!=PK_EVOLVE_INTENT_MISSING)return false;
    if(t->fail_create){t->status=PK_EVOLVE_INTENT_CORRUPT;return false;}
    t->intent=*i;t->status=PK_EVOLVE_INTENT_PENDING;return true;
}
static bool complete(void *ctx,const pk_evolve_tx_intent_t *i){
    Test *t=(Test*)ctx;
    if(t->fail_complete)return false;
    if(t->status!=PK_EVOLVE_INTENT_PENDING ||
       memcmp(&t->intent,i,sizeof(*i)))return false;
    t->status=PK_EVOLVE_INTENT_COMMITTED;return true;
}
static bool load(void *ctx,uint32_t id,pk_profile_t *o){
    Test *t=(Test*)ctx;if(id!=184199)return false;
    *o=t->profile;return true;
}
static bool save(void *ctx,uint32_t id,const pk_profile_t *before,
                 const pk_profile_t *after){
    Test *t=(Test*)ctx;
    if(id!=184199 || t->fail_save ||
       memcmp(&t->profile,before,sizeof(*before)))return false;
    t->profile=*after;t->profile_writes++;return true;
}
static Test init(void){
    Test t={0};t.status=PK_EVOLVE_INTENT_MISSING;
    pk_profile_init(&t.profile);assert(pk_team_add(&t.profile,161,true));
    t.profile.team[0].level=30;t.profile.team[0].xp=187;
    return t;
}
static pk_evolve_tx_io_t io(Test *t){
    pk_evolve_tx_io_t x={t,get,create,complete,load,save};return x;
}
int main(void){
    Test t=init();pk_evolve_tx_io_t x=io(&t);
    assert(pk_evolve_tx_begin(&x,184199,11,0,162)==PK_EVOLVE_TX_DONE);
    assert(t.profile.team[0].species==162 && t.profile.team[0].level==30);
    assert(t.profile.team[0].xp==187 && t.profile.team[0].shiny==1);
    assert(pk_dex_has(&t.profile,162,true));
    assert(t.profile_writes==1);
    assert(pk_evolve_tx_recover(&x,184199)==PK_EVOLVE_TX_ALREADY_DONE);
    assert(pk_evolve_tx_begin(&x,184199,12,0,162)==PK_EVOLVE_TX_BLOCKED);
    t=init();x=io(&t);t.fail_save=1;
    assert(pk_evolve_tx_begin(&x,184199,19,0,162)==PK_EVOLVE_TX_RECOVERY_NEEDED);
    assert(t.status==PK_EVOLVE_INTENT_PENDING);
    assert(t.profile.team[0].species==161);
    t.fail_save=0;
    assert(pk_evolve_tx_recover(&x,184199)==PK_EVOLVE_TX_DONE);
    assert(t.profile_writes==1);
    t=init();x=io(&t);t.fail_complete=1;
    assert(pk_evolve_tx_begin(&x,184199,20,0,162)==PK_EVOLVE_TX_RECOVERY_NEEDED);
    assert(t.profile_writes==1);
    t.fail_complete=0;
    assert(pk_evolve_tx_recover(&x,184199)==PK_EVOLVE_TX_DONE);
    assert(t.profile_writes==1);
    t=init();x=io(&t);t.fail_save=1;
    assert(pk_evolve_tx_begin(&x,184199,21,0,162)==PK_EVOLVE_TX_RECOVERY_NEEDED);
    /* An unrelated XP update makes a stale intent unsafe to replay. */
    t.profile.team[0].xp++;
    t.fail_save=0;
    assert(pk_evolve_tx_recover(&x,184199)==PK_EVOLVE_TX_RECOVERY_NEEDED);
    assert(t.profile.team[0].species==161 && t.profile_writes==0);
    t=init();x=io(&t);
    assert(pk_evolve_tx_begin(&x,184199,22,0,134)==PK_EVOLVE_TX_BLOCKED);
    assert(pk_evolve_tx_begin(&x,184199,23,0,162)==PK_EVOLVE_TX_DONE);
    puts("Evolution level transaction and crash recovery PASS");
}
