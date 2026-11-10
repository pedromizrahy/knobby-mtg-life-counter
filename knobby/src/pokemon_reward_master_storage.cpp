#include "pokemon_reward_master_storage.h"
#include "pokemon_master_trainer_intent.h"
#include "pokemon_profile_store.h"
#include "pokemon_reward_ledger.h"

static pk_ledger_state_t get_state(void *ctx,uint32_t uid,uint32_t game){
    (void)ctx;return pk_ledger_get(uid,game);
}
static bool record_state(void *ctx,uint32_t uid,uint32_t game,pk_ledger_state_t state){
    (void)ctx;return pk_ledger_record(uid,game,state);
}
static bool load_profile(void *ctx,uint32_t uid,pk_profile_t *p){
    (void)ctx;return pk_store_load(uid,p);
}
static bool save_profile(void *ctx,uint32_t uid,const pk_profile_t *p){
    (void)ctx;return pk_store_save(uid,p);
}
static bool find_pending(void *ctx,uint32_t uid,uint32_t *game){
    (void)ctx;return pk_ledger_find_pending(uid,game);
}
static pk_mt_intent_status_t intent_status(void *ctx,uint32_t uid,uint32_t *game){
    (void)ctx;return pk_mt_intent_status(uid,game);
}
static bool intent_prepare(void *ctx,uint32_t uid,uint32_t game,
                           const pk_profile_t *before,const pk_profile_t *after){
    (void)ctx;return pk_mt_intent_prepare(uid,game,before,after);
}
static bool intent_finalize(void *ctx,uint32_t uid,uint32_t game){
    (void)ctx;
    const pk_mt_recovery_io_t io={nullptr,get_state,load_profile};
    return pk_mt_intent_finalize(uid,game,&io);
}
extern "C" pk_tx_result_t pk_reward_master_apply_existing(uint32_t uid,
                uint32_t game,bool won,pk_reward_t *out)
{
    const pk_reward_master_io_t io={
        {nullptr,get_state,record_state,load_profile,save_profile,find_pending},
        intent_status,intent_prepare,intent_finalize
    };
    return pk_reward_master_transaction(&io,uid,game,won,out);
}
