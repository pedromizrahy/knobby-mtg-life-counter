#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_companion_profile_view.h"
typedef struct {
    pk_profile_t profile;
    uint32_t expected;
    unsigned reads;
    int fail;
} stub_t;
static bool load(void *context,uint32_t id,pk_profile_t *out)
{
    stub_t *s=context;
    ++s->reads;
    if(s->fail || id!=s->expected)return false;
    *out=s->profile;
    return true;
}
int main(void)
{
    stub_t m={0};
    pk_profile_init(&m.profile);
    assert(pk_team_add(&m.profile,778,false));
    m.profile.team[0].level=23;
    m.profile.team[0].xp=42;
    m.expected=123;
    pk_companion_view_t v={999,999,99,9,999};
    assert(pk_companion_read_view(load,&m,123,778,&v));
    assert(v.user_id==123 && v.species==778 && v.level==23 &&
           v.shiny==0 && v.xp==42 && m.reads==1);
    const pk_companion_view_t snapshot=v;
    assert(!pk_companion_read_view(load,&m,124,778,&v));
    assert(!memcmp(&snapshot,&v,sizeof(v)));
    assert(!pk_companion_read_view(load,&m,0,778,&v));
    assert(!pk_companion_read_view(load,&m,0xFFFFFFFCU,778,&v));
    assert(!pk_companion_read_view(load,&m,123,25,&v));
    assert(!pk_companion_read_view(load,&m,123,0,&v));
    assert(!pk_companion_read_view(load,&m,123,1026,&v));
    assert(!pk_companion_read_view(NULL,&m,123,778,&v));
    assert(!pk_companion_read_view(load,&m,123,778,NULL));
    assert(!memcmp(&snapshot,&v,sizeof(v)));
    m.fail=1;
    assert(!pk_companion_read_view(load,&m,123,778,&v));
    m.fail=0;
    m.profile.version=2;
    assert(!pk_companion_read_view(load,&m,123,778,&v));
    m.profile.version=1;
    m.profile.team[0].shiny=2;
    assert(!pk_companion_read_view(load,&m,123,778,&v));
    m.profile.team[0].shiny=1;
    assert(pk_companion_read_view(load,&m,123,778,&v) && v.shiny==1);
    puts("Pokemon authenticated profile preview bridge tests passed");
    return 0;
}
