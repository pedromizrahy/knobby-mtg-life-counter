#include <cassert>
#include <cstdio>
#include <cstring>
#include "SPIFFS.h"
#include "../knobby/src/pokemon_item_store.h"
MockSPIFFS SPIFFS;
int main(){
    const uint32_t user=184199;
    pk_item_ledger_t ledger={},check={};
    uint32_t generation=0;
    assert(pk_item_store_inspect(user,&check,&generation)==PK_ITEM_STORE_MISSING);
    assert(pk_item_register_mon(&ledger,0xCAFEBABE00000001ULL));
    assert(pk_item_store_create(user,&ledger));
    assert(!pk_item_store_create(user,&ledger));
    assert(pk_item_store_inspect(user,&check,&generation)==PK_ITEM_STORE_VALID);
    assert(generation==1);
    assert(pk_item_level_sync(&check,0xCAFEBABE00000001ULL,30));
    assert(pk_item_claim(&check,0xCAFEBABE00000001ULL,0,PK_ITEM_WATER_STONE));
    assert(pk_item_store_update(user,generation,&check));
    assert(!pk_item_store_update(user,generation,&check));
    assert(pk_item_store_inspect(user,&ledger,&generation)==PK_ITEM_STORE_VALID);
    assert(generation==2 && ledger.count[PK_ITEM_WATER_STONE]==1);
    /* No second payout after restart. */
    auto before=ledger;
    assert(!pk_item_claim(&ledger,0xCAFEBABE00000001ULL,0,PK_ITEM_FIRE_STONE));
    assert(!memcmp(&before,&ledger,sizeof(before)));
    /* No claim history reset. */
    ledger.mons[0].claimed_mask=0;
    assert(!pk_item_store_update(user,generation,&ledger));
    ledger=before;
    /* A write failure corrupts the attempted slot: fail CLOSED.
       Never recover an old ledger and refund spent resources. */
    SPIFFS.fail_write=true;
    assert(!pk_item_store_update(user,generation,&ledger));
    SPIFFS.fail_write=false;
    assert(pk_item_store_inspect(user,&check,&generation)==PK_ITEM_STORE_CORRUPT);
    assert(!pk_item_store_update(user,2,&ledger));
    assert(!pk_item_store_create(user,&ledger));
    assert(pk_item_store_inspect(55,&check,&generation)==PK_ITEM_STORE_MISSING);
    /* Independent accounts cannot access each other's inventory. */
    assert(pk_item_store_create(55,&before));
    assert(pk_item_store_inspect(55,&check,&generation)==PK_ITEM_STORE_VALID);
    assert(check.count[PK_ITEM_WATER_STONE]==1);
    SPIFFS.mount_ok=false;
    assert(pk_item_store_inspect(56,&check,&generation)==PK_ITEM_STORE_ERROR);
    assert(!pk_item_store_create(56,&before));
    puts("Persistent evolutionary inventory safety tests passed");
}
