#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../knobby/src/pokemon_gacha.h"

typedef struct {
    uint32_t values[8];
    unsigned length;
    unsigned offset;
    bool fail;
} rng_state_t;
static bool next(void *ctx,uint32_t *out)
{
    rng_state_t *s=ctx;
    if(s->fail || s->offset>=s->length)return false;
    *out=s->values[s->offset++];
    return true;
}
static bool draw(uint8_t ball,uint32_t roll,
                 const pk_gacha_entry_t *pool,size_t count,
                 pk_gacha_result_t *out)
{
    rng_state_t rng={{10000U+roll,0,4097},3,0,false};
    return pk_gacha_preview(ball,pool,count,next,&rng,out);
}
int main(void)
{
    const pk_gacha_entry_t pool[]={
      {25,PK_TIER_COMMON,1},
      {778,PK_TIER_RARE,1},
      {150,PK_TIER_EPIC,1},
      {151,PK_TIER_LEGENDARY,1}
    };
    const uint16_t expected[PK_BALL_COUNT][PK_TIER_COUNT]={
        {7500,2000,450,50},
        {5500,3000,1300,200},
        {3000,4000,2400,600},
        {0,0,0,10000}
    };
    for(unsigned ball=0;ball<PK_BALL_COUNT;ball++){
        unsigned cumulative=0;
        for(unsigned tier=0;tier<PK_TIER_COUNT;tier++){
            assert(pk_gacha_weight(ball,tier)==expected[ball][tier]);
            unsigned width=expected[ball][tier];
            if(width){
                pk_gacha_result_t result={0};
                assert(draw(ball,cumulative,pool,4,&result));
                assert(result.tier==tier);
                assert(result.species==pool[tier].species);
                assert(result.shiny==0);
                assert(draw(ball,cumulative+width-1,pool,4,&result));
                assert(result.tier==tier);
            }
            cumulative+=width;
        }
        assert(cumulative==10000);
    }
    pk_gacha_result_t result={0};
    assert(draw(PK_BALL_MASTER,0,pool,4,&result));
    assert(result.tier==PK_TIER_LEGENDARY);

    /* Independent shiny roll, same Pokemon and tier. */
    rng_state_t shiny={{10000,0,4096},3,0,false};
    assert(pk_gacha_preview(PK_BALL_POKE,pool,4,next,&shiny,&result));
    assert(result.species==25 && result.shiny==1);

    /* Missing selected rarity cannot silently become another tier. */
    pk_gacha_result_t unchanged={999,7,7};
    const pk_gacha_entry_t starter[]={ {778,PK_TIER_RARE,1} };
    assert(!draw(PK_BALL_MASTER,0,starter,1,&unchanged));
    assert(unchanged.species==999 && unchanged.tier==7 && unchanged.shiny==7);
    assert(!draw(PK_BALL_POKE,0,starter,1,&unchanged));
    assert(!draw(PK_BALL_MASTER,0,NULL,0,&unchanged));
    assert(!draw(PK_BALL_COUNT,0,pool,4,&unchanged));
    pk_gacha_entry_t bad[]={ {25,PK_TIER_COMMON,1},
                              {25,PK_TIER_RARE,1} };
    assert(!draw(PK_BALL_POKE,0,bad,2,&unchanged));
    bad[1].species=151;bad[1].sprite_ready=0;
    assert(!draw(PK_BALL_POKE,0,bad,2,&unchanged));

    rng_state_t failed={{10000,0,4097},3,0,true};
    assert(!pk_gacha_preview(PK_BALL_POKE,pool,4,next,&failed,&unchanged));
    rng_state_t truncated={{10000},1,0,false};
    assert(!pk_gacha_preview(PK_BALL_POKE,pool,4,next,&truncated,&unchanged));
    assert(unchanged.species==999);

    /* Rejection samples below threshold without modulo bias. */
    rng_state_t retry={{0,10000,0,4097},4,0,false};
    assert(pk_gacha_preview(PK_BALL_POKE,pool,4,next,&retry,&result));
    assert(retry.offset==4 && result.tier==PK_TIER_COMMON);

    puts("Pokemon gacha pure sampler tests passed");
    return 0;
}
