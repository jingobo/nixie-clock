#ifndef __EVENT_H
#define __EVENT_H

#include "system.h"

// Ѕазовый класс событи¤
class event_base_t
{
protected:
    // “ип дл¤ частички очереди
    typedef uint8_t queue_item_t;
    // »спользуема¤ очередь дл¤ имитации событий
    const xQueueHandle queue;
public:
    //  онструктор по умолчанию
    event_base_t(void) : queue(xQueueCreate(1, sizeof(queue_item_t)))
    { }

    // ”становка событи¤
    ROM void set(void)
    {
        queue_item_t dummy;
        xQueueSend(queue, &dummy, 0);
    }

    // ”становка событи¤ (из под прерывани¤)
    ROM void set_isr(void)
    {
        queue_item_t dummy;
        xQueueSendFromISR(queue, &dummy, 0);
    }

    // ќжидание событи¤
    ROM virtual bool wait(uint32_t mills = portMAX_DELAY) = 0;
};

// —обытие с автосбросом
class event_auto_t : public event_base_t
{
public:
    // ќжидание событи¤
    virtual bool wait(uint32_t mills = portMAX_DELAY);
};

// —обытие с ручным сбросом
class event_manual_t : public event_base_t
{
public:
    // ќжидание событи¤
    virtual bool wait(uint32_t mills = portMAX_DELAY);

    // —брос событи¤
    ROM void reset(void)
    {
        xQueueReset(queue);
    }
};

#endif // __EVENT_H
