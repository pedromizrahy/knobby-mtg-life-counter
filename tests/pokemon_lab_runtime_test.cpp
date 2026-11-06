#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../knobby/src/pokemon_lab_runtime.h"
int main()
{
    pk_lab_t snap={};
    uint32_t initial=0;
    assert(pk_lab_runtime_session_token(&initial));
    assert(!pk_lab_runtime_draw(PK_BALL_POKE,1));
    assert(pk_lab_runtime_start());
    uint32_t first=0;
    assert(pk_lab_runtime_session_token(&first) && first!=initial);
    uint32_t draw_session=0;
    pk_lab_t drawn={};
    assert(pk_lab_runtime_draw_snapshot_token(PK_BALL_MASTER,1,&drawn,&draw_session));
    assert(draw_session==first);
    assert(drawn.draws[PK_BALL_MASTER]==1);
    assert(drawn.last.tier==PK_TIER_LEGENDARY);
    const pk_gacha_result_t frozen=drawn.last;
    assert(pk_lab_runtime_draw(PK_BALL_MASTER,8));
    assert(pk_lab_runtime_snapshot(&snap));
    assert(snap.draws[PK_BALL_MASTER]==9);
    assert(drawn.draws[PK_BALL_MASTER]==1);
    assert(std::memcmp(&frozen,&drawn.last,sizeof(frozen))==0);
    uint32_t still_first=0;
    assert(pk_lab_runtime_session_token(&still_first) && still_first==first);
    assert(!pk_lab_runtime_draw_snapshot_token(PK_BALL_COUNT,1,&drawn,&draw_session));
    assert(draw_session==first && drawn.draws[PK_BALL_MASTER]==1);
    assert(pk_lab_runtime_badge());
    assert(pk_lab_runtime_level(100));
    assert(pk_lab_runtime_snapshot(&snap));
    assert(snap.master_trainer_badge && snap.simulated_level==100);
    assert(pk_lab_runtime_reset());
    uint32_t second=0;
    assert(pk_lab_runtime_session_token(&second) && second!=first);
    assert(pk_lab_runtime_snapshot(&snap) && !snap.active);
    assert(snap.draws[PK_BALL_MASTER]==0);
    assert(!pk_lab_runtime_draw(PK_BALL_MASTER,1));
    assert(pk_lab_runtime_start());
    uint32_t third=0;
    assert(pk_lab_runtime_session_token(&third) && third!=second);
    assert(!pk_lab_runtime_draw_snapshot_token(PK_BALL_MASTER,1,nullptr,&draw_session));
    assert(!pk_lab_runtime_draw_snapshot_token(PK_BALL_MASTER,1,&drawn,nullptr));
    assert(!pk_lab_runtime_session_token(nullptr));
    assert(pk_lab_runtime_snapshot(&snap) && snap.active && !snap.has_last);
    assert(pk_lab_runtime_draw_snapshot_token(PK_BALL_POKE,100,&drawn,&draw_session));
    assert(draw_session==third);
    assert(drawn.draws[PK_BALL_POKE]==100);
    assert(pk_lab_runtime_snapshot(&snap) && snap.draws[PK_BALL_POKE]==100);
    std::puts("Pokemon LAB runtime session and stale-reveal tests passed");
    return 0;
}
