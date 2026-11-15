#include "pokemon_starter_service.h"
#include "pokemon_file_store.h"
#include "pokemon_player_identity.h"
#include <string.h>

/* A kept result is committed before profile creation. If power fails between
 * those two steps, retry KEEP to complete the same result; reroll is forbidden.
 * This module cannot grant an arbitrary starter to a different Playgroup ID:
 * only a caller with a freshly authenticated matching /me may invoke it. */
static bool safe_account(uint32_t id,pk_profile_t *profile)
{
    if(!pk_player_id_valid(id))return false;
    if(pk_file_inspect(id,profile)!=PK_STORE_MISSING)return false;
    if(pk_store_inspect(id,NULL)!=PK_STORE_MISSING)return false;
    return true;
}
static pk_starter_result_t finish(uint32_t id,
                                  const pk_starter_state_t *state)
{
    if(!state->kept || !pk_starter_state_valid(state))
        return PK_STARTER_BLOCKED;
    pk_profile_t existing={};
    pk_store_status_t file=pk_file_inspect(id,&existing);
    pk_store_status_t nvs=pk_store_inspect(id,NULL);
    if(nvs!=PK_STORE_MISSING)return PK_STARTER_BLOCKED;
    if(file==PK_STORE_VALID){
        /* A previous successful creation may have survived a reset. */
        if(existing.version==1U && existing.team_count==1U &&
           existing.active_index==0U &&
           existing.team[0].species==state->current.species &&
           existing.team[0].shiny==state->current.shiny)
            return PK_STARTER_ALREADY_OWNED;
        return PK_STARTER_BLOCKED;
    }
    if(file!=PK_STORE_MISSING)return PK_STARTER_BLOCKED;
    if(!pk_file_create_selected_starter(id,state->current.species,
                                         state->current.shiny))
        return PK_STARTER_FAILED;
    pk_profile_t verify={};
    if(pk_file_inspect(id,&verify)!=PK_STORE_VALID ||
       verify.team_count!=1U || verify.active_index!=0U ||
       verify.team[0].species!=state->current.species ||
       verify.team[0].shiny!=state->current.shiny ||
       verify.team[0].level!=1U)
        return PK_STARTER_FAILED;
    return PK_STARTER_ACCEPTED;
}
extern "C" pk_starter_result_t pk_starter_service_open(
    uint32_t id,const pk_gacha_entry_t *catalog,size_t count,
    pk_gacha_rng_t rng,void *ctx,pk_starter_state_t *out)
{
    if(!out || !pk_player_id_valid(id))return PK_STARTER_BLOCKED;
    pk_starter_state_t saved={};
    pk_starter_store_status_t status=pk_starter_store_inspect(id,&saved);
    if(status==PK_STARTER_STORE_VALID){
        if(!pk_starter_state_valid(&saved))return PK_STARTER_BLOCKED;
        *out=saved;
        return saved.kept?finish(id,&saved):PK_STARTER_REVEALED;
    }
    if(status!=PK_STARTER_STORE_MISSING)return PK_STARTER_BLOCKED;
    if(!safe_account(id,NULL))return PK_STARTER_BLOCKED;
    pk_starter_state_t fresh={};
    if(!pk_starter_begin(&fresh,catalog,count,rng,ctx))
        return PK_STARTER_FAILED;
    if(!pk_starter_store_create(id,&fresh))
        return PK_STARTER_FAILED;
    *out=fresh;
    return PK_STARTER_REVEALED;
}
extern "C" pk_starter_result_t pk_starter_service_reroll(
    uint32_t id,const pk_gacha_entry_t *catalog,size_t count,
    pk_gacha_rng_t rng,void *ctx,pk_starter_state_t *out)
{
    if(!out || !pk_player_id_valid(id) || !safe_account(id,NULL))
        return PK_STARTER_BLOCKED;
    pk_starter_state_t saved={};
    if(pk_starter_store_inspect(id,&saved)!=PK_STARTER_STORE_VALID ||
       saved.kept)return PK_STARTER_BLOCKED;
    pk_starter_state_t next=saved;
    if(!pk_starter_reroll(&next,catalog,count,rng,ctx))
        return PK_STARTER_BLOCKED;
    if(!pk_starter_store_update(id,&next))return PK_STARTER_FAILED;
    *out=next;
    return PK_STARTER_REVEALED;
}
extern "C" pk_starter_result_t pk_starter_service_keep(
    uint32_t id,pk_starter_state_t *out)
{
    if(!out || !pk_player_id_valid(id))return PK_STARTER_BLOCKED;
    pk_starter_state_t saved={};
    if(pk_starter_store_inspect(id,&saved)!=PK_STARTER_STORE_VALID)
        return PK_STARTER_BLOCKED;
    if(!saved.kept){
        if(!safe_account(id,NULL))return PK_STARTER_BLOCKED;
        pk_gacha_result_t chosen={};
        if(!pk_starter_keep(&saved,&chosen) ||
           !pk_starter_store_update(id,&saved))
            return PK_STARTER_FAILED;
    }
    *out=saved;
    return finish(id,&saved);
}
