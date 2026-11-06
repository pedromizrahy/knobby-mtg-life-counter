#include "pokemon_companion_profile_view.h"
#include "pokemon_companion_selection.h"
#include "pokemon_player_identity.h"
bool pk_companion_read_view(pk_companion_profile_load_t load,void *ctx,
                            uint32_t user_id,uint16_t species,
                            pk_companion_view_t *out)
{
    if(!load || !out || !pk_player_id_valid(user_id) ||
       species<1U || species>PK_DEX_SPECIES) return false;
    pk_profile_t profile;
    if(!load(ctx,user_id,&profile) ||
       !pk_companion_is_owned(&profile,species))return false;
    for(unsigned i=0;i<profile.team_count;i++){
        const pk_mon_t *mon=&profile.team[i];
        if(mon->species!=species)continue;
        if(mon->shiny>1U || mon->level==0U || mon->level>100U)
            return false;
        const pk_companion_view_t result={
            user_id,species,mon->level,mon->shiny,mon->xp
        };
        *out=result;
        return true;
    }
    return false;
}
