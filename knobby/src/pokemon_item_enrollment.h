#ifndef POKEMON_ITEM_ENROLLMENT_H
#define POKEMON_ITEM_ENROLLMENT_H
#include <stdint.h>
#include "pokemon_item_store.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    PK_ITEM_ENROLL_FAILED=0,
    PK_ITEM_ENROLL_CREATED=1,
    PK_ITEM_ENROLL_ALREADY=2,
    PK_ITEM_ENROLL_BLOCKED=3
} pk_item_enroll_result_t;
/* No caller should invoke this until fresh /me resolves to user_id and
 * the game is not active. No auto-enrollment during boot or rerolls.
 * id must be an independently generated nonzero secure 64-bit value. */
pk_item_enroll_result_t pk_item_enroll_confirmed_starter(
    uint32_t authenticated_user_id,uint64_t unique_instance_id,
    pk_item_ledger_t *out);
#ifdef __cplusplus
}
#endif
#endif
