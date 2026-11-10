#include "pokemon_progress.h"
#include <limits.h>
#include <string.h>
#define PK_VERSION 1U
static bool valid_species(uint16_t id) { return id>=1 && id<=PK_DEX_SPECIES; }
static uint8_t bit_for(uint16_t id) { return (uint8_t)(1U<<((id-1U)&7U)); }
static unsigned offset_for(uint16_t id) { return (unsigned)(id-1U)/8U; }
/* Commander-paced leveling: 21,780 XP from level 1 to 100. */
static uint32_t xp_for_next(uint8_t level) { return 20U+(uint32_t)level*4U; }
void pk_profile_init(pk_profile_t *p) {
    if(!p) return;
    memset(p,0,sizeof(*p));
    p->version=PK_VERSION;
}
bool pk_dex_has(const pk_profile_t *p, uint16_t id, bool shiny) {
    if(!p || !valid_species(id)) return false;
    return ((shiny?p->shiny_dex:p->dex)[offset_for(id)] & bit_for(id))!=0;
}
bool pk_register_discovery(pk_profile_t *p,uint16_t id,bool shiny) {
    if(!p || !valid_species(id)) return false;
    bool was=pk_dex_has(p,id,shiny);
    p->dex[offset_for(id)] |= bit_for(id);
    if(shiny) p->shiny_dex[offset_for(id)] |= bit_for(id);
    return !was;
}
uint16_t pk_dex_count(const pk_profile_t *p) {
    if(!p) return 0;
    uint16_t count=0;
    for(uint16_t id=1;id<=PK_DEX_SPECIES;id++) if(pk_dex_has(p,id,false)) count++;
    return count;
}
bool pk_team_add(pk_profile_t *p,uint16_t species,bool shiny) {
    if(!p || !valid_species(species) || p->team_count>=PK_TEAM_MAX) return false;
    pk_mon_t *m=&p->team[p->team_count++];
    m->species=species; m->shiny=shiny?1U:0U; m->level=1; m->xp=0;
    pk_register_discovery(p,species,shiny);
    return true;
}
bool pk_award_game(pk_profile_t *p,uint32_t id,bool won,pk_reward_t *out) {
    if(!p || !out || id==0 || p->version!=PK_VERSION || p->team_count==0 ||
       p->active_index>=p->team_count) return false;
    for(unsigned i=0;i<PK_RECENT_GAMES;i++) if(p->recent_game_ids[i]==id) return false;
    if(p->battles==UINT32_MAX || (won && p->wins==UINT32_MAX)) return false;
    memset(out,0,sizeof(*out));
    p->recent_game_ids[p->recent_next]=id;
    p->recent_next=(uint8_t)((p->recent_next+1U)%PK_RECENT_GAMES);
    p->battles++;
    if(won) {
        p->wins++;
        if(p->balls[PK_BALL_POKE]<UINT16_MAX) {
            p->balls[PK_BALL_POKE]++;out->new_pokeballs++;
        }
        if(p->wins%5U==0 && p->balls[PK_BALL_ULTRA]<UINT16_MAX) {
            p->balls[PK_BALL_ULTRA]++;out->new_ultraballs++;
        }
    }
    pk_mon_t *m=&p->team[p->active_index];
    uint32_t grant=won?800U:300U;
    out->xp_awarded=(uint16_t)grant;
    if(m->level<100) {
        m->xp=(m->xp>UINT32_MAX-grant)?UINT32_MAX:m->xp+grant;
        while(m->level<100 && m->xp>=xp_for_next(m->level)) {
            m->xp-=xp_for_next(m->level);
            m->level++;out->levels_gained++;
            if(m->level%5U==0 && p->balls[PK_BALL_POKE]<UINT16_MAX) {
                p->balls[PK_BALL_POKE]++;out->new_pokeballs++;
            }
            if(m->level==50 && p->balls[PK_BALL_ULTRA]<UINT16_MAX) {
                p->balls[PK_BALL_ULTRA]++;out->new_ultraballs++;
            }
            /* Master Ball is reserved for the once-per-player Master Trainer
             * achievement. Do not award per-Pokemon: the persistent unlock
             * marker and crash-safe claim still need their own integration. */
        }
    }
    return true;
}
