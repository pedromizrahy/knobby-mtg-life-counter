#include "pokemon_playgroup_roster.h"
#include "pokemon_player_identity.h"
#include <string.h>
static bool profile_valid(const pk_profile_t *p){
    if(!p || p->version!=1U || p->team_count>PK_TEAM_MAX ||
       p->recent_next>=PK_RECENT_GAMES ||
       p->team_count==0 || p->active_index>=p->team_count)
        return false;
    const pk_mon_t *m=&p->team[p->active_index];
    return m->species>=1U && m->species<=PK_DEX_SPECIES &&
           m->shiny<=1U && m->level>=1U && m->level<=100U;
}
bool pk_pg_collect_main(const pk_pg_seat_t *seats,size_t count,
                        pk_pg_load_profile_t load,void *ctx,
                        pk_pg_main_t *out,size_t capacity)
{
    if(!seats || !out || !load || count<1U || count>8U ||
       capacity<count)return false;
    /* Validate the whole input before modifying any caller data. */
    for(size_t i=0;i<count;i++){
        if(memchr(seats[i].name,0,sizeof(seats[i].name))==NULL)
            return false;
    }
    memset(out,0,count*sizeof(*out));
    for(size_t i=0;i<count;i++){
        memcpy(out[i].name,seats[i].name,sizeof(out[i].name));
        if(!pk_player_id_valid(seats[i].user_id))
            continue; /* guest or missing identity */
        bool ambiguous=false;
        for(size_t j=0;j<count;j++)
            if(i!=j && seats[i].user_id==seats[j].user_id)
                ambiguous=true;
        if(ambiguous)continue; /* duplicate ID cannot claim a profile */
        pk_profile_t profile;
        if(!load(ctx,(uint32_t)seats[i].user_id,&profile) ||
           !profile_valid(&profile))continue;
        const pk_mon_t *m=&profile.team[profile.active_index];
        out[i].user_id=(uint32_t)seats[i].user_id;
        out[i].species=m->species;
        out[i].shiny=m->shiny;
        out[i].level=m->level;
        out[i].xp=m->xp;
        out[i].has_main=true;
    }
    return true;
}
