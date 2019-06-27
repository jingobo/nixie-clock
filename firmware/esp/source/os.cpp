﻿#include "os.h"
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
    if (!OS_CHECK_HANDLE(mutex))
        return;
    vSemaphoreDelete(mutex);
    mutex = NULL;
}

void os_mutex_t::enter(void) const
{
    if (!xSemaphoreTakeRecursive(mutex, portMAX_DELAY))
        LOGE("Unable to enter mutex!");
}

RAM void os_mutex_t::enter_irq(void) const
{
    if (!xSemaphoreTakeFromISR(mutex, NULL))
        LOGE("Unable to enter mutex(from isr)!");
}

void os_mutex_t::leave(void) const
{
    if (!xSemaphoreGiveRecursive(mutex))
        LOGE("Unable to leave mutex!");
}

RAM void os_mutex_t::leave_irq(void) const
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

RAM void os_event_base_t::set(void)
{
    queue_item_t item;
    xQueueSend(queue, &item, 0);
}

RAM void os_event_base_t::set_isr(void)
{
    queue_item_t item;
    xQueueSendFromISR(queue, &item, NULL);
}

RAM bool os_event_auto_t::wait(uint32_t mills)
{
    queue_item_t item;
    return xQueueReceive(queue, &item, OS_MS_TO_TICKS(mills)) > 0;
}

RAM bool os_event_manual_t::wait(uint32_t mills)
{
    queue_item_t item;
    return xQueuePeek(queue, &item, OS_MS_TO_TICKS(mills)) > 0;
}

RAM void os_event_manual_t::reset(void)
{
    queue_item_t item;
    xQueueReceive(queue, &item, 0);
}

RAM void os_event_manual_t::reset_isr(void)
{
    queue_item_t item;
    xQueueReceiveFromISR(queue, &item, NULL);
}

// --- Task --- //

os_task_base_t::os_task_base_t(const char * _name) : active(false), restart(false), name(_name)
{
    assert(name != NULL);
}

RAM void os_task_base_t::task_routine(void *arg)
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
    return result;
}

RAM void os_task_base_t::priority_set(os_task_priority_t priority)
{
    vTaskPrioritySet(NULL, (unsigned)priority);
}