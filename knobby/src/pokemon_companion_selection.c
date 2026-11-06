#include "pokemon_companion_selection.h"

static bool valid(const pk_profile_t *p)
{
    return p && p->version==1U && p->team_count<=PK_TEAM_MAX &&
           p->recent_next<PK_RECENT_GAMES &&
           (p->team_count==0 || p->active_index<p->team_count);
}
bool pk_companion_is_owned(const pk_profile_t *p,uint16_t species)
{
    return valid(p) && species>=1U && species<=PK_DEX_SPECIES &&
           pk_dex_has(p,species,false);
}
uint16_t pk_companion_next_owned(const pk_profile_t *p,
                                 uint16_t from,int direction)
{
    if(!valid(p) || (direction!=1 && direction!=-1) ||
       from>PK_DEX_SPECIES)return 0;
    int position=(int)from;
    if(from==0)position=direction==1?0:PK_DEX_SPECIES+1;
    for(unsigned i=0;i<PK_DEX_SPECIES;i++){
        position+=direction;
        if(position<1)position=PK_DEX_SPECIES;
        if(position>PK_DEX_SPECIES)position=1;
        if(pk_companion_is_owned(p,(uint16_t)position))
            return (uint16_t)position;
    }
    return 0;
}
