#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../knobby/src/pokemon_evolution_items.h"
int main(void){
    pk_item_ledger_t ledger={0};
    assert(pk_item_milestone_level(0)==30);
    assert(pk_item_milestone_level(1)==50);
    assert(pk_item_milestone_level(2)==75);
    assert(pk_item_milestone_group(0)==PK_ITEM_GROUP_STONE);
    assert(pk_item_milestone_group(1)==PK_ITEM_GROUP_SPECIAL);
    assert(pk_item_milestone_group(2)==PK_ITEM_GROUP_ANY);
    assert(!pk_item_register_mon(&ledger,0));
    assert(pk_item_register_mon(&ledger,101));
    assert(!pk_item_register_mon(&ledger,101));
    assert(!pk_item_claim(&ledger,101,0,PK_ITEM_FIRE_STONE));
    assert(pk_item_level_sync(&ledger,101,29));
    assert(!pk_item_claim(&ledger,101,0,PK_ITEM_FIRE_STONE));
    assert(pk_item_level_sync(&ledger,101,30));
    assert(!pk_item_claim(&ledger,101,0,PK_ITEM_LINK_CABLE));
    assert(pk_item_claim(&ledger,101,0,PK_ITEM_WATER_STONE));
    assert(ledger.count[PK_ITEM_WATER_STONE]==1);
    pk_item_ledger_t before=ledger;
    assert(!pk_item_claim(&ledger,101,0,PK_ITEM_WATER_STONE));
    assert(!memcmp(&before,&ledger,sizeof(ledger)));
    assert(pk_item_level_sync(&ledger,101,80));
    assert(pk_item_claim(&ledger,101,1,PK_ITEM_LINK_CABLE));
    assert(pk_item_claim(&ledger,101,2,PK_ITEM_METAL_COAT));
    assert(!pk_item_claim(&ledger,101,2,PK_ITEM_METAL_COAT));
    assert(pk_item_consume(&ledger,PK_ITEM_WATER_STONE));
    assert(!pk_item_consume(&ledger,PK_ITEM_WATER_STONE));
    assert(ledger.count[PK_ITEM_LINK_CABLE]==1);
    /* Simulated evolution or party reorder does not reset claims:
     * the immutable instance ID stays the same. */
    assert(pk_item_level_sync(&ledger,101,100));
    assert(!pk_item_claim(&ledger,101,0,PK_ITEM_FIRE_STONE));
    assert(pk_item_register_mon(&ledger,102));
    assert(pk_item_level_sync(&ledger,102,30));
    assert(pk_item_claim(&ledger,102,0,PK_ITEM_FIRE_STONE));
    assert(ledger.count[PK_ITEM_FIRE_STONE]==1);
    assert(!pk_item_claim(&ledger,999,0,PK_ITEM_FIRE_STONE));
    assert(pk_item_name(PK_ITEM_LINK_CABLE)!=NULL);
    assert(!pk_item_name(PK_ITEM_NONE));
    puts("Pokemon item milestone ledger PASS");
    return 0;
}
