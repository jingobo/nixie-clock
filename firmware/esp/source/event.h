#ifndef __EVENT_H
#define __EVENT_H

#include "system.h"

// Базовый класс события
class event_base_t
{
protected:
    // Тип для частички очереди
    typedef uint8_t queue_item_t;
    // Используемая очередь для имитации событий
    const xQueueHandle queue;
public:
    // Конструктор по умолчанию
    event_base_t(void) : queue(xQueueCreate(1, sizeof(queue_item_t)))
    { }

    // Установка события
    ROM void set(void)
    {
        queue_item_t dummy;
        xQueueSend(queue, &dummy, 0);
    }

    // Установка события (из под прерывания)
    ROM void set_isr(void)
    {
        queue_item_t dummy;
        xQueueSendFromISR(queue, &dummy, 0);
    }

    // Ожидание события
    ROM virtual bool wait(uint32_t mills = portMAX_DELAY) = 0;
};

// Событие с автосбросом
class event_auto_t : public event_base_t
{
public:
    // Ожидание события
    virtual bool wait(uint32_t mills = portMAX_DELAY);
};

// Событие с ручным сбросом
class event_manual_t : public event_base_t
{
public:
    // Ожидание события
    virtual bool wait(uint32_t mills = portMAX_DELAY);

    // Сброс события
    ROM void reset(void)
    {
        xQueueReset(queue);
    }
};

#endif // __EVENT_H
