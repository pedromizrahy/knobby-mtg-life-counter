#ifndef PK_MOCK_SEMPHR_H
#define PK_MOCK_SEMPHR_H
#include "FreeRTOS.h"
/* Host-only timed mutex exercises real contention between USB/UI callers.
 * Never linked into ESP32 firmware. */
#include <chrono>
#include <mutex>
#include <new>
typedef void *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return new (std::nothrow) std::timed_mutex();
}
static inline void vSemaphoreDelete(SemaphoreHandle_t mutex) {
    delete static_cast<std::timed_mutex *>(mutex);
}
static inline int xSemaphoreTake(SemaphoreHandle_t mutex, unsigned ticks) {
    if(!mutex)return pdFALSE;
    bool ok=static_cast<std::timed_mutex *>(mutex)->try_lock_for(
        std::chrono::milliseconds(ticks));
    return ok?pdTRUE:pdFALSE;
}
static inline int xSemaphoreGive(SemaphoreHandle_t mutex) {
    if(!mutex)return pdFALSE;
    static_cast<std::timed_mutex *>(mutex)->unlock();
    return pdTRUE;
}
#endif
