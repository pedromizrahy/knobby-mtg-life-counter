#include "pokemon_pc.h"
static bool mon_ok(const pk_mon_t *m){
    return m && m->species>=1U && m->species<=PK_DEX_SPECIES &&
           m->shiny<=1U && m->level>=1U && m->level<=100U;
}
static bool team_ok(const pk_profile_t *p){
    if(!p || p->version!=1U || p->team_count>PK_TEAM_MAX ||
       p->recent_next>=PK_RECENT_GAMES ||
       (p->team_count? p->active_index>=p->team_count :
                        p->active_index!=0U))return false;
    for(unsigned i=0;i<p->team_count;i++)
        if(!mon_ok(&p->team[i]))return false;
    return true;
}
static bool box_ok(const pk_pc_t *pc){
    if(!pc || pc->count>pc->capacity ||
       (pc->capacity && !pc->entries))return false;
    for(size_t i=0;i<pc->count;i++)
        if(!mon_ok(&pc->entries[i]))return false;
    return true;
}
bool pk_pc_set_main(pk_profile_t *p,unsigned slot){
    if(!team_ok(p) || slot>=p->team_count)return false;
    p->active_index=(uint8_t)slot;
    return true;
}
bool pk_pc_add_captured(pk_pc_t *pc,pk_mon_t mon){
    if(!box_ok(pc) || !mon_ok(&mon) || pc->count>=pc->capacity)
        return false;
    pc->entries[pc->count++]=mon;
    return true;
}
bool pk_pc_swap_team(pk_profile_t *p,pk_pc_t *pc,
                     unsigned team_slot,size_t pc_slot){
    if(!team_ok(p) || !box_ok(pc) ||
       team_slot>=p->team_count || pc_slot>=pc->count)return false;
    pk_mon_t old=p->team[team_slot];
    p->team[team_slot]=pc->entries[pc_slot];
    pc->entries[pc_slot]=old;
    return true;
}
bool pk_pc_take_team(pk_profile_t *p,pk_pc_t *pc,size_t pc_slot){
    if(!team_ok(p) || !box_ok(pc) || p->team_count>=PK_TEAM_MAX ||
       pc_slot>=pc->count)return false;
    p->team[p->team_count++]=pc->entries[pc_slot];
    for(size_t i=pc_slot+1U;i<pc->count;i++)
        pc->entries[i-1U]=pc->entries[i];
    pc->count--;
    if(p->team_count==1U)p->active_index=0U;
    return true;
}
bool pk_pc_deposit_team(pk_profile_t *p,pk_pc_t *pc,
                        unsigned team_slot){
    if(!team_ok(p) || !box_ok(pc) || p->team_count<=1U ||
       team_slot>=p->team_count || team_slot==p->active_index ||
       pc->count>=pc->capacity)return false;
    pc->entries[pc->count++]=p->team[team_slot];
    for(unsigned i=team_slot+1U;i<p->team_count;i++)
        p->team[i-1U]=p->team[i];
    p->team_count--;
    if(team_slot<p->active_index)p->active_index--;
    return true;
}
