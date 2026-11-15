#include "pokemon_reward_storage.h"
#include "pokemon_profile_store.h"
#include "pokemon_reward_ledger.h"

/* Bound to real NVS + SPIFFS storage. No side effects unless the exported
 * opt-in function is explicitly called. Must never be invoked from a live
 * Magic game or before the Playgroup result snapshot is durable.
 * NOTE: this adapter does not initialize new profiles or resolve guests.
 */
static pk_ledger_state_t state_get(void *ctx,uint32_t user_id,uint32_t session)
{
    (void)ctx;
    return pk_ledger_get(user_id,session);
}
static bool state_record(void *ctx,uint32_t user_id,uint32_t session,
                         pk_ledger_state_t state)
{
    (void)ctx;
    return pk_ledger_record(user_id,session,state);
}
static bool profile_load(void *ctx,uint32_t user_id,pk_profile_t *out)
{
    (void)ctx;
    return pk_store_load(user_id,out);
}
static bool profile_save(void *ctx,uint32_t user_id,const pk_profile_t *profile)
{
    (void)ctx;
    return pk_store_save(user_id,profile);
}
static bool pending_find(void *ctx,uint32_t user_id,uint32_t *out)
{
    (void)ctx;
    return pk_ledger_find_pending(user_id,out);
}
extern "C" pk_tx_result_t pk_reward_apply_existing(uint32_t user_id,
             uint32_t session_id,bool won,pk_reward_t *out)
{
    const pk_reward_io_t io={
        nullptr,state_get,state_record,profile_load,profile_save,pending_find
    };
    return pk_reward_transaction(&io,user_id,session_id,won,out);
}
