#include "pokemon_lab_runtime.h"
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
namespace {
SemaphoreHandle_t lab_lock=nullptr;
pk_lab_t lab={};
bool ensure_lock()
{
    if(lab_lock)return true;
    /* Initialized at explicit use, not a timer or background task.
     * The firmware's UI and USB command dispatcher are initialized
     * before users can call this API. */
    lab_lock=xSemaphoreCreateMutex();
    return lab_lock!=nullptr;
}
bool enter()
{
    return ensure_lock() && xSemaphoreTake(lab_lock,pdMS_TO_TICKS(150))==pdTRUE;
}
void leave(){xSemaphoreGive(lab_lock);}
bool rng(void *,uint32_t *out)
{
    if(!out)return false;
    *out=esp_random();
    return true;
}
}
extern "C" bool pk_lab_runtime_start(void)
{
    if(!enter())return false;
    pk_lab_start(&lab);leave();return true;
}
extern "C" bool pk_lab_runtime_reset(void)
{
    if(!enter())return false;
    pk_lab_reset(&lab);leave();return true;
}
extern "C" bool pk_lab_runtime_snapshot(pk_lab_t *out)
{
    if(!out || !enter())return false;
    *out=lab;leave();return true;
}
extern "C" bool pk_lab_runtime_draw(uint8_t ball,unsigned count)
{
    if(!enter())return false;
    bool ok=pk_lab_draw(&lab,ball,count,rng,nullptr);
    leave();return ok;
}
extern "C" bool pk_lab_runtime_badge(void)
{
    if(!enter())return false;
    bool ok=pk_lab_award_master_badge(&lab);
    leave();return ok;
}
extern "C" bool pk_lab_runtime_level(uint8_t level)
{
    if(!enter())return false;
    bool ok=pk_lab_set_level(&lab,level);
    leave();return ok;
}
