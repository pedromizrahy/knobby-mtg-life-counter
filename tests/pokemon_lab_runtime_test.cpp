#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>
#include "../knobby/src/pokemon_lab_runtime.h"
int main()
{
    /* Concurrent first access must publish one mutex. */
    std::atomic<bool> go{false};
    std::atomic<unsigned> initial_ok{0};
    std::vector<std::thread> starters;
    for(unsigned i=0;i<12;i++)
        starters.emplace_back([&]{
            while(!go.load(std::memory_order_acquire))
                std::this_thread::yield();
            uint32_t token=0;
            if(pk_lab_runtime_session_token(&token))initial_ok++;
        });
    go.store(true,std::memory_order_release);
    for(auto &thread:starters)thread.join();
    assert(initial_ok==12);
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
    /* Real host mutex: 4 concurrent consumers cannot lose increments. */
    assert(pk_lab_runtime_start());
    std::atomic<unsigned> failures{0};
    std::vector<std::thread> clients;
    for(unsigned i=0;i<4;i++)
        clients.emplace_back([&]{
            for(unsigned j=0;j<40;j++)
                if(!pk_lab_runtime_draw(PK_BALL_MASTER,1))failures++;
        });
    for(auto &thread:clients)thread.join();
    assert(failures==0);
    assert(pk_lab_runtime_snapshot(&snap));
    assert(snap.draws[PK_BALL_MASTER]==160);
    assert(snap.tiers[PK_BALL_MASTER][PK_TIER_LEGENDARY]==160);
    std::puts("Pokemon LAB runtime session and stale-reveal tests passed");
    return 0;
}
