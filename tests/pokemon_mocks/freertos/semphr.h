#ifndef PK_MOCK_SEMPHR_H
#define PK_MOCK_SEMPHR_H
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    static int fake_mutex;
    return &fake_mutex;
}
static inline void vSemaphoreDelete(SemaphoreHandle_t mutex) {(void)mutex;}
static inline int xSemaphoreTake(SemaphoreHandle_t mutex, unsigned ticks) {
    (void)ticks;return mutex?pdTRUE:pdFALSE;
}
static inline int xSemaphoreGive(SemaphoreHandle_t mutex) {
    return mutex?pdTRUE:pdFALSE;
}
#ifdef __cplusplus
}
#endif
#endif
