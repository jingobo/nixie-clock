#include "event.h"
#include "nvic.h"
#include "system.h"

// ����� ���������� ���������� ��������
#define EVENT_TIMER_COUNT               10
// ���������� ����������� �� ���
#define EVENT_TIMER_US_PER_TICK         4
// ���������� ������� ������� �������
#define EVENT_TIMER_FREQUENCY_HZ        (XM(1) / EVENT_TIMER_US_PER_TICK)

// �������� ��� ������� �� ���������� �������
#define EVENT_TIMER_NOT_FOUND           UINT8_MAX

// �������� ���������� (��������)
#if EVENT_TIMER_COUNT >= EVENT_TIMER_NOT_FOUND
    #error software timer count too large
#endif

// ��� ������ ��� �������� ������� � �����
typedef uint16_t event_timer_period_t;

// ����� �������� ������� ��� ����������� �������
#define EVENT_TIMER_PERIOD_RAW          ((event_timer_period_t)-1)
// ����������� ������ � �����
#define EVENT_TIMER_PERIOD_MIN          ((event_timer_period_t)2)
// ������������ ������ � �����
#define EVENT_TIMER_PERIOD_MAX          (EVENT_TIMER_PERIOD_RAW - EVENT_TIMER_PERIOD_MIN)

// ���������� ��������� �������
#define EVENT_TIMER_STATE_DECLARE(n)    MASK(event_t::timer_t::state_t, 1, n)
#define EVENT_TIMER_STATE_NONE          ((event_t::timer_t::state_t)0)  // ��������� �����������
#define EVENT_TIMER_STATE_ACTIVE        EVENT_TIMER_STATE_DECLARE(0)    // ������ �������
#define EVENT_TIMER_STATE_PENDING       EVENT_TIMER_STATE_DECLARE(1)    // ������ ��������
#define EVENT_TIMER_STATE_CIRQ          EVENT_TIMER_STATE_DECLARE(2)    // ��������� ������� �� ����������
// ��������� ��������� �������
#define EVENT_TIMER_STATE_USED          (EVENT_TIMER_STATE_ACTIVE | EVENT_TIMER_STATE_PENDING)

// ����� ���������� �������
static struct event_t
{
    // ������ ��� ���������� ��������
    class timer_t : event_base_t
    {
        // �������� ��������� �������
        typedef uint8_t state_t;
        
        // �������� �������
        struct slot_t
        {
            // �������� ����������
            event_interval_t current, reload;
            // ���������� �������
            notify_t *handler;
            // ���������
            state_t state;
            
            // ����������� �� ���������
            slot_t(void) : state(EVENT_TIMER_STATE_NONE)
            { }
        } slot[EVENT_TIMER_COUNT];
        // ��������� �������� ����������� ��������
        event_timer_period_t counter;
        
        // ����� ���������� ������� � ������ ������ (��������� �� ������� ������������)
        uint8_t find_free(void) const
        {
            for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
                if (!(slot[i].state & EVENT_TIMER_STATE_USED))
                    return i;
            return EVENT_TIMER_NOT_FOUND;
        }

        // ����� ���������� ������� � ����� ������
        uint8_t find_free_back(void) const
        {
            for (uint8_t i = EVENT_TIMER_COUNT; i > 0;)
                if (!(slot[--i].state & EVENT_TIMER_STATE_USED))
                    return i;
            return EVENT_TIMER_NOT_FOUND;
        }

        // ����� ������� �� �����������
        uint8_t find_handler(const notify_t &handler) const
        {
            for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
                if (slot[i].handler == &handler)
                    return i;
            return EVENT_TIMER_NOT_FOUND;
        }
    protected:
        // ���������� ����� ��������
        virtual void notify_event(void)
        {
            // ���������� ��������� ����������
            IRQ_CTX_SAVE();
            // ��������� ����� ��������
            for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
                if (slot[i].state & EVENT_TIMER_STATE_PENDING)
                {
                    // ���������� ����
                    slot[i].handler->notify_event();
                    // ������ ��������� �������� ���������
                    IRQ_CTX_DISABLE();
                        slot[i].state &= ~EVENT_TIMER_STATE_PENDING;
                    IRQ_CTX_RESTORE();
                }
        }
    public:
        // ����������� �� ���������
        timer_t(void) : counter(0)
        { }

        // ���������� ���������� ����������� �������
        INLINE_FORCED
        OPTIMIZE_SPEED
        void interrupt_handler(void)
        {
            bool event_gen = false;
            event_timer_period_t dx, dt, ccr = EVENT_TIMER_PERIOD_MAX;
            // ����������� ������� ������� � �����
            dx = (event_timer_period_t)TIM3->CNT;
            dx -= counter;
            counter += dx;
            // ��������� ��������
            for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
            {
                // ���� ������ �������� - �������
                if (!(slot[i].state & EVENT_TIMER_STATE_ACTIVE))
                    continue;
                if (slot[i].current <= dx)
                {
                    // ������������� �������
                    if (slot[i].state & EVENT_TIMER_STATE_CIRQ)
                        // ...����� �� ����������
                        slot[i].handler->notify_event();
                    else
                    {
                        // ...� �������� ����
                        event_gen = true;
                        slot[i].state |= EVENT_TIMER_STATE_PENDING;
                    }
                    if (slot[i].reload <= 0)
                    {
                        // ����������
                        slot[i].state &= ~EVENT_TIMER_STATE_ACTIVE;
                        continue;
                    }
                    // ���������� ������� ������� ����������
                    dt = dx - slot[i].current;
                    // ������������ ���������� ������� �� �������� ������������
                    while (dt >= slot[i].reload)
                        dt -= slot[i].reload;
                    // ���������� ��������� ��������� ������������
                    slot[i].current = slot[i].reload;
                    // �������� ����������������� ���������� �����
                    slot[i].current -= dt;
                }
                else
                    slot[i].current -= dx;
                // ����������� ������� ���������� ������������ ����������� �������
                if (ccr > slot[i].current)
                    ccr = slot[i].current;
            }
            // ������� ������� ���������� ������������
            if (ccr < EVENT_TIMER_PERIOD_MIN)
                ccr = EVENT_TIMER_PERIOD_MIN;
            // ��������� �������� CCR1
            ccr_set(ccr);
            // ��������� �������
            if (event_gen)
                event_add(this);
        }
        
        // ��������� ������ �������� ��� �������� �������/���������
        OPTIMIZE_SPEED
        void ccr_set(event_timer_period_t delta) const
        {
            delta += (event_timer_period_t)TIM3->CNT;
            // ��������� �������� CCR1
            TIM3->CCR1 = delta;
            // ����� ����� ����������
            TIM3->SR &= ~TIM_SR_CC1IF;                                          // Clear IRQ CC1 pending flag
        }

        // ����� ������������ ������� �� ��������� ���������� �����
        void start(notify_t &handler, event_interval_t interval, event_timer_flag_t flags)
        {
            // �������� ����������
            if (interval <= 0)
                return;
            // ��������� ��� ����������
            IRQ_SAFE_ENTER();
                // ����� �� �����������
                uint8_t i = find_handler(handler);
                if (i == EVENT_TIMER_NOT_FOUND)
                {
                    // ����� ���������� �����
                    i = (flags & EVENT_TIMER_FLAG_HEAD) ? find_free() : find_free_back();
                    // ����� ���� �� ������ - ��� �����������
                    assert(i != EVENT_TIMER_NOT_FOUND);
                }
                // ��������� �����
                    // ����������, ��������
                slot[i].current = interval;
                slot[i].handler = &handler;
                    // ��������� (��� ����� ��������)
                slot[i].state |= EVENT_TIMER_STATE_ACTIVE;
                if (flags & EVENT_TIMER_FLAG_CIRQ)
                    slot[i].state |= EVENT_TIMER_STATE_CIRQ;
                else
                    slot[i].state &= ~EVENT_TIMER_STATE_CIRQ;
                    // ������
                slot[i].reload = (flags & EVENT_TIMER_FLAG_LOOP) ? interval : 0;
            // �������������� ����������
            IRQ_SAFE_LEAVE();
            // ������������ ������������ ����������
            ccr_set(EVENT_TIMER_PERIOD_MIN);
        }

        // ���� ������������ ������� (���������� - ������� �� ������)
        INLINE_FORCED
        bool stop(const notify_t &handler)
        {
            bool result = false;
            // ��������� ��� ����������
            IRQ_SAFE_ENTER();
                // ����� �����
                uint8_t i = find_handler(handler);
                if (i != EVENT_TIMER_NOT_FOUND)
                {
                    // ����������
                    slot[i].state &= ~EVENT_TIMER_STATE_ACTIVE;
                    result = true;
                }
            // �������������� ����������
            IRQ_SAFE_LEAVE();
            return result;
        }
    } timer;
    
    // ������ ����� �������
    class scope_t
    {
        list_template_t<event_base_t> list[2];
        list_template_t<event_base_t> *active;
    public:
        // �����������
        scope_t(void) : active(list)
        { }
        
        // ���������� ������� � �������
        INLINE_FORCED
        void add(event_base_t &event)
        {
            // ���������� � ������ ������ �������
            IRQ_SAFE_ENTER();
                // ���� ������� ��� � �������� - �������
                if (event.pending_set())
                    event.link(*active, LIST_SIDE_HEAD);
            IRQ_SAFE_LEAVE();
        }
        
        // ���������� ������� �������
        INLINE_FORCED
        void execute(void)
        {
            // ���������, ������ �� ������
            if (active->empty())
                return;
            uint8_t i;
            IRQ_SAFE_ENTER();
                // ���������� ����� ������ ��� �������
                if (active == list + 0)
                    i = 0;
                else if (active == list + 1)
                    i = 1;
                // �������� �������� ������� ������
                active = list + (i ^ 1);
                assert(active->empty());
            IRQ_SAFE_LEAVE();
            // ��������� �������
            do
            {
                // �������� ������ ������� ������
                event_base_t &event = *list[i].head();
                // ��������� �������
                event.notify();
                // ������� �� ������
                event.unlink();
                // C��������� ���� ��������
                IRQ_CTX_DISABLE();
                    event.pending_reset();
                IRQ_CTX_RESTORE();
            } while (!list[i].empty());
        }
    } scope;
} event;

void event_init(void)
{
    // ������������ � ����� �������
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;                                         // TIM3 clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM3RST;                                      // TIM3 reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM3RST;                                     // TIM3 unreset
    // ���������������� �������
    TIM3->CCMR1 = TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1;                          // CC1 output, CC1 Fast off, CC1 preload off, CC1 mode Toggle (011)
    TIM3->PSC = FMCU_NORMAL_HZ / EVENT_TIMER_FREQUENCY_HZ - 1;                  // Prescaler (frequency)
    TIM3->ARR = EVENT_TIMER_PERIOD_RAW;                                         // Max period
    TIM3->DIER = TIM_DIER_CC1IE;                                                // CC1 IRQ enable
    // ���������������� ����������
    nvic_irq_enable_set(TIM3_IRQn, true);                                       // TIM3 IRQ enable
    nvic_irq_priority_set(TIM3_IRQn, NVIC_IRQ_PRIORITY_HIGHEST);                // Middle TIM3 IRQ priority
    // ����� ����������� �������
    event.timer.ccr_set(EVENT_TIMER_PERIOD_MAX);
    TIM3->CR1 |= TIM_CR1_CEN;                                                   // TIM enable
}

void event_add(event_base_t &ev)
{
    event.scope.add(ev);
}

void event_add(event_base_t *ev)
{
    assert(ev != NULL);
    event_add(*ev);
}

void event_execute(void)
{
    event.scope.execute();
}

bool event_timer_stop(const notify_t &handler)
{
    return event.timer.stop(handler);
}

void event_timer_start_us(notify_t &handler, event_interval_t us, event_timer_flag_t flags)
{
    event.timer.start(handler, us / EVENT_TIMER_US_PER_TICK, flags);
}

void event_timer_start_hz(notify_t &handler, float_t hz, event_timer_flag_t flags)
{
    assert(hz > 0);
    assert(hz <= EVENT_TIMER_FREQUENCY_HZ);
    event.timer.start(handler, (event_interval_t)(EVENT_TIMER_FREQUENCY_HZ / hz), flags);
}

IRQ_ROUTINE
void event_interrupt_timer(void)
{
    event.timer.interrupt_handler();
}
