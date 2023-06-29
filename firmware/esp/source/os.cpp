#include "os.h"
#include "log.h"

LOG_TAG_DECL("OS");

// --- Mutex --- //

os_mutex_t::os_mutex_t(void)
{
    mutex = xSemaphoreCreateRecursiveMutex();
    assert(OS_CHECK_HANDLE(mutex));
}

os_mutex_t::~os_mutex_t(void)
{
    if (OS_CHECK_HANDLE(mutex))
        vSemaphoreDelete(mutex);
}

RAM_GCC
void os_mutex_t::enter(void) const
{
    if (!xSemaphoreTakeRecursive(mutex, portMAX_DELAY))
        LOGE("Unable to enter mutex!");
}

RAM_GCC
void os_mutex_t::enter_irq(void) const
{
    if (!xSemaphoreTakeFromISR(mutex, NULL))
        LOGE("Unable to enter mutex(from isr)!");
}

RAM_GCC
void os_mutex_t::leave(void) const
{
    if (!xSemaphoreGiveRecursive(mutex))
        LOGE("Unable to leave mutex!");
}

RAM_GCC
void os_mutex_t::leave_irq(void) const
{
    if (!xSemaphoreGiveFromISR(mutex, NULL))
        LOGE("Unable to leave mutex(from isr)!");
}

// --- Event --- //

os_event_base_t::os_event_base_t(void) : queue(xQueueCreate(1, sizeof(queue_item_t)))
{
    assert(OS_CHECK_HANDLE(queue));
}

os_event_base_t::~os_event_base_t(void)
{
    if (OS_CHECK_HANDLE(queue))
        vQueueDelete(queue);
}

RAM_GCC
void os_event_base_t::set(void)
{
    queue_item_t item;
    xQueueSend(queue, &item, OS_TICK_MIN);
}

RAM_GCC
void os_event_base_t::set_isr(void)
{
    queue_item_t item;
    BaseType_t switch_task;
    xQueueSendFromISR(queue, &item, &switch_task);
    if (switch_task)
        taskYIELD();
}

RAM_GCC
bool os_event_auto_t::wait(os_tick_t ticks)
{
    queue_item_t item;
    return xQueueReceive(queue, &item, ticks) > 0;
}

RAM_GCC
bool os_event_manual_t::wait(os_tick_t ticks)
{
    queue_item_t item;
    return xQueuePeek(queue, &item, ticks) > 0;
}

RAM_GCC
void os_event_manual_t::reset(void)
{
    queue_item_t item;
    xQueueReceive(queue, &item, OS_TICK_MIN);
}

RAM_GCC
void os_event_manual_t::reset_isr(void)
{
    queue_item_t item;
    BaseType_t switch_task;
    xQueueReceiveFromISR(queue, &item, &switch_task);
    if (switch_task)
        taskYIELD();
}

os_event_group_t::os_event_group_t(void) : handle(xEventGroupCreate())
{
    assert(OS_CHECK_HANDLE(handle));
}

os_event_group_t::~os_event_group_t(void)
{
    if (OS_CHECK_HANDLE(handle))
        vEventGroupDelete(handle);
}

RAM_GCC
void os_event_group_t::set(uint32_t bits)
{
    assert(bits > 0);
    xEventGroupSetBits(handle, bits);
}

RAM_GCC
void os_event_group_t::reset(uint32_t bits)
{
    assert(bits > 0);
    xEventGroupClearBits(handle, bits);
}

RAM_GCC
bool os_event_group_t::wait(uint32_t bits, bool all, os_tick_t ticks)
{
    assert(bits > 0);
    return xEventGroupWaitBits(handle, bits, pdFALSE, all ? pdTRUE : pdFALSE, ticks) > 0;
}

// --- Task --- //

os_task_base_t::os_task_base_t(const char * _name, bool _critical)
    : name(_name), critical(_critical)
{
    assert(name != NULL);
}

RAM_GCC
void os_task_base_t::task_routine(void *arg)
{
    ((os_task_base_t *)arg)->execute_wrapper();
    vTaskDelete(NULL);
}

// Оболочка для метода выполнения задачи
void os_task_base_t::execute_wrapper(void)
{
    volatile bool rst;
    // Выполнение
    execute();
    // Снятие флага выполнения
    mutex.enter();
        active = false;
        rst = restart;
        restart = false;
    mutex.leave();
    LOGI("Stopped \"%s\"...", name);
    // Если нужно задачу перезапустить
    if (rst && start())
        LOGI("Restarted \"%s\"...", name);
}

RAM_GCC
void os_task_base_t::delay(os_tick_t ticks)
{
    vTaskDelay(ticks);
}

bool os_task_base_t::start(bool auto_restart)
{
    auto result = false;
    mutex.enter();
        // Проверка активности задачи
        if (active)
        {
            if (auto_restart)
                restart = true;
        }
        else if (xTaskCreate(task_routine, name, 4096, this, OS_TASK_PRIORITY_NORMAL, NULL) != pdPASS)
            LOGI("Create \"%s\" failed!", name);
        else
        {
            LOGI("Started \"%s\"...", name);
            result = active = true;
        }
        LOGH();
    mutex.leave();
    if (critical && !result)
    {
        LOGE("Critical task \"%s\" run failed!", name);
        abort();
    }
    return result;
}

RAM_GCC
void os_task_base_t::priority_set(os_task_priority_t priority)
{
    vTaskPrioritySet(NULL, (unsigned)priority);
}

RAM_GCC
os_tick_t os_tick_get(void)
{
    return (os_tick_t)xTaskGetTickCount();
}
