#include <assert.h>
#include <stdio.h>
#include "../knobby/src/pokemon_account_onboarding.h"
int main(void) {
    assert(pk_onboard_decide(0,PK_STORE_MISSING,PK_STORE_MISSING)==PK_ONBOARD_INVALID);
    assert(pk_onboard_decide(0xFFFFFFFCULL,PK_STORE_MISSING,PK_STORE_MISSING)==PK_ONBOARD_INVALID);
    assert(pk_onboard_decide(1234567,PK_STORE_MISSING,PK_STORE_MISSING)==PK_ONBOARD_READY);
    assert(pk_onboard_decide(1234567,PK_STORE_VALID,PK_STORE_MISSING)==PK_ONBOARD_ALREADY_FILE);
    assert(pk_onboard_decide(1234567,PK_STORE_MISSING,PK_STORE_VALID)==PK_ONBOARD_LEGACY_NVS);
    assert(pk_onboard_decide(1234567,PK_STORE_VALID,PK_STORE_VALID)==PK_ONBOARD_ALREADY_FILE);
    assert(pk_onboard_decide(1234567,PK_STORE_CORRUPT,PK_STORE_MISSING)==PK_ONBOARD_UNSAFE);
    assert(pk_onboard_decide(1234567,PK_STORE_MISSING,PK_STORE_CORRUPT)==PK_ONBOARD_UNSAFE);
    assert(pk_onboard_decide(1234567,PK_STORE_ERROR,PK_STORE_MISSING)==PK_ONBOARD_UNSAFE);
    assert(pk_onboard_decide(1234567,PK_STORE_MISSING,PK_STORE_ERROR)==PK_ONBOARD_UNSAFE);
    puts("Authenticated account onboarding policy passed");
    return 0;
}
