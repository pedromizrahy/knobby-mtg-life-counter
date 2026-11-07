#include "pokemon_player_identity.h"
bool pk_player_id_valid(uint64_t user_id)
{
    return user_id!=0U && user_id<0xFFFFFFFCULL;
}
bool pk_player_id_in_roster(uint64_t requested,
                            const uint64_t *roster,unsigned count)
{
    if(!pk_player_id_valid(requested) || !roster ||
       count<2U || count>8U)return false;
    bool found=false;
    for(unsigned i=0;i<count;i++){
        if(!pk_player_id_valid(roster[i]))return false;
        for(unsigned j=0;j<i;j++)
            if(roster[j]==roster[i])return false;
        if(roster[i]==requested)found=true;
    }
    return found;
}
