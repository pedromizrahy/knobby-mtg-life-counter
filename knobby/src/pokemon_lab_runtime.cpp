#include "pokemon_lab_runtime.h"
#include <esp_random.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
namespace {
/* USB and LVGL can both be first to enter. Publish exactly ONE mutex
 * before either uses it; discard a concurrent duplicate safely. */
std::atomic<SemaphoreHandle_t> lab_lock{nullptr};
pk_lab_t lab={};
uint32_t lab_session=0;
bool ensure_lock()
{
    if(lab_lock.load(std::memory_order_acquire)!=nullptr)return true;
    SemaphoreHandle_t created=xSemaphoreCreateMutex();
    if(!created)return false;
    SemaphoreHandle_t expected=nullptr;
    if(!lab_lock.compare_exchange_strong(expected,created,
           std::memory_order_release,std::memory_order_acquire))
        vSemaphoreDelete(created);
    return true;
}
bool enter()
{
    if(!ensure_lock())return false;
    SemaphoreHandle_t current=lab_lock.load(std::memory_order_acquire);
    return current && xSemaphoreTake(current,pdMS_TO_TICKS(150))==pdTRUE;
}
void leave()
{
    SemaphoreHandle_t current=lab_lock.load(std::memory_order_acquire);
    if(current)xSemaphoreGive(current);
}
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
    pk_lab_start(&lab);
    lab_session++;
    leave();return true;
}
extern "C" bool pk_lab_runtime_reset(void)
{
    if(!enter())return false;
    pk_lab_reset(&lab);
    lab_session++;
    leave();return true;
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
extern "C" bool pk_lab_runtime_draw_snapshot(uint8_t ball,unsigned count,
                                                pk_lab_t *out)
{
    if(!out || !enter())return false;
    const bool ok=pk_lab_draw(&lab,ball,count,rng,nullptr);
    if(ok)*out=lab;
    leave();
    return ok;
}
extern "C" bool pk_lab_runtime_session_token(uint32_t *out)
{
    if(!out || !enter())return false;
    *out=lab_session;
    leave();return true;
}
extern "C" bool pk_lab_runtime_draw_snapshot_token(uint8_t ball,
             unsigned count,pk_lab_t *out,uint32_t *token)
{
    if(!out || !token || !enter())return false;
    bool ok=pk_lab_draw(&lab,ball,count,rng,nullptr);
    if(ok){*out=lab;*token=lab_session;}
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
