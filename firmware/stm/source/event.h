#ifndef __EVENT_H
#define __EVENT_H

#include <list.h>

// ����� �������� �������
class event_base_t : public list_item_t, public notify_t
{
    friend class event_t;
    // ���������, ��� ������� �������� � ������� ���������
    bool pending;
public:
    // ����������� �� ���������
    event_base_t(void) : pending(false)
    { }
protected:
    // ��������� ����� �������� ���������
    virtual bool pending_set(void)
    {
        // ���� ��� ���������� - �������
        if (pending)
            return false;
        // ������������
        pending = true;
        return true;
    }

    // ����� �������� ���������
    virtual void pending_reset(void)
    {
        pending = false;
    }
    
    // ����� �������
    virtual void notify(void)
    {
        // �������� ���������
        assert(pending);
        // ����� �������
        notify_event();
    }
};

// ������������� ������ (���������� �����)
void event_init(void);
// ���������� ������� �������
void event_execute(void);
// ���������� ������� � �������
void event_add(event_base_t &ev);
void event_add(event_base_t *ev);

// ����� ��� �������� �������
typedef uint8_t event_timer_flag_t;
#define EVENT_TIMER_FLAG_DECLARE(n)     MASK(event_timer_flag_t, 1, n)
#define EVENT_TIMER_FLAG_NONE           ((event_timer_flag_t)0)     // ����� ������ �����
#define EVENT_TIMER_FLAG_LOOP           EVENT_TIMER_FLAG_DECLARE(0) // ������ �����������
#define EVENT_TIMER_FLAG_HEAD           EVENT_TIMER_FLAG_DECLARE(1) // �������������� � ������ �������
#define EVENT_TIMER_FLAG_CIRQ           EVENT_TIMER_FLAG_DECLARE(2) // ���������� �� ����������

// ���������� ��������� �������
#define EVENT_TIMER_PRI_DEFAULT         EVENT_TIMER_FLAG_NONE
#define EVENT_TIMER_PRI_HIGHEST         EVENT_TIMER_FLAG_HEAD
#define EVENT_TIMER_PRI_CRITICAL        (EVENT_TIMER_FLAG_HEAD | EVENT_TIMER_FLAG_CIRQ)

// ��� ������ ��� �������� ��������� � �����
typedef uint32_t event_interval_t;

// ����� ������������ �������
void event_timer_start_hz(notify_t &handler, float_t hz, event_timer_flag_t flags = EVENT_TIMER_FLAG_NONE);
void event_timer_start_us(notify_t &handler, event_interval_t us, event_timer_flag_t flags = EVENT_TIMER_FLAG_NONE);
// ���� ������������ ������� (���������� - ������� �� ������)
bool event_timer_stop(const notify_t &handler);

// ���������� ���������� ����������� �������
void event_interrupt_timer(void);

#endif // __EVENT_H
