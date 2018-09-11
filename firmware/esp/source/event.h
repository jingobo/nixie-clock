#ifndef __EVENT_H
#define __EVENT_H

#include "system.h"

// ������� ����� �������
class event_base_t
{
protected:
    // ��� ��� �������� �������
    typedef uint8_t queue_item_t;
    // ������������ ������� ��� �������� �������
    const xQueueHandle queue;
public:
    // ����������� �� ���������
    event_base_t(void) : queue(xQueueCreate(1, sizeof(queue_item_t)))
    { }

    // ��������� �������
    ROM void set(void)
    {
        queue_item_t dummy;
        xQueueSend(queue, &dummy, 0);
    }

    // ��������� ������� (�� ��� ����������)
    ROM void set_isr(void)
    {
        queue_item_t dummy;
        xQueueSendFromISR(queue, &dummy, 0);
    }

    // �������� �������
    ROM virtual bool wait(uint32_t mills = portMAX_DELAY) = 0;
};

// ������� � �����������
class event_auto_t : public event_base_t
{
public:
    // �������� �������
    virtual bool wait(uint32_t mills = portMAX_DELAY);
};

// ������� � ������ �������
class event_manual_t : public event_base_t
{
public:
    // �������� �������
    virtual bool wait(uint32_t mills = portMAX_DELAY);

    // ����� �������
    ROM void reset(void)
    {
        xQueueReset(queue);
    }
};

#endif // __EVENT_H
