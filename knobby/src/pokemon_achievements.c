#include "pokemon_achievements.h"
#include <stdint.h>

static bool valid_team(const pk_profile_t *p)
{
    if (!p || p->version!=1U || p->team_count==0 ||
        p->team_count>PK_TEAM_MAX || p->active_index>=p->team_count)
        return false;
    for (unsigned i=0;i<p->team_count;i++) {
        if (p->team[i].species==0 || p->team[i].species>PK_DEX_SPECIES ||
            p->team[i].level==0 || p->team[i].level>100)
            return false;
    }
    return true;
}

bool pk_master_trainer_first_level_100(const pk_profile_t *before,
                                       const pk_profile_t *after)
{
    if (!valid_team(before) || !valid_team(after) ||
        before->team_count!=after->team_count ||
        before->active_index!=after->active_index ||
        before->battles==UINT32_MAX ||
        after->battles!=before->battles+1U)
        return false;

    unsigned active=before->active_index;
    for (unsigned i=0;i<before->team_count;i++) {
        if (before->team[i].species!=after->team[i].species ||
            before->team[i].shiny!=after->team[i].shiny)
            return false;
        /* A Master Trainer award requires crossing level 100 for the
         * FIRST time in the profile, not leveling another team member. */
        if (before->team[i].level==100) return false;
        if (i!=active && before->team[i].level!=after->team[i].level)
            return false;
    }
    return before->team[active].level<100 &&
           after->team[active].level==100;
}
