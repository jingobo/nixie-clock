#include "event.h"
#include "nvic.h"
#include "system.h"

// Общее количество програмных таймеров
#define EVENT_TIMER_COUNT               10
// Количество микросекунд на тик
#define EVENT_TIMER_US_PER_TICK         4
// Аппаратная частота таймера событий
#define EVENT_TIMER_FREQUENCY_HZ        (XM(1) / EVENT_TIMER_US_PER_TICK)

// Значение для индекса не найденного таймера
#define EVENT_TIMER_NOT_FOUND           UINT8_MAX

// Проверка количества (контракт)
#if EVENT_TIMER_COUNT >= EVENT_TIMER_NOT_FOUND
    #error software timer count too large
#endif

// Тип данных для хранения пероида в тиках
typedef uint16_t event_timer_period_t;

// Сырое значение периода для аппаратного таймера
#define EVENT_TIMER_PERIOD_RAW          ((event_timer_period_t)-1)
// Минимальный пероид в тиках
#define EVENT_TIMER_PERIOD_MIN          ((event_timer_period_t)2)
// Максимальный пероид в тиках
#define EVENT_TIMER_PERIOD_MAX          (EVENT_TIMER_PERIOD_RAW - EVENT_TIMER_PERIOD_MIN)

// Объявление состояний таймера
#define EVENT_TIMER_STATE_DECLARE(n)    MASK(event_t::timer_t::state_t, 1, n)
#define EVENT_TIMER_STATE_NONE          ((event_t::timer_t::state_t)0)  // Состояния отсутствуют
#define EVENT_TIMER_STATE_ACTIVE        EVENT_TIMER_STATE_DECLARE(0)    // Таймер запущен
#define EVENT_TIMER_STATE_PENDING       EVENT_TIMER_STATE_DECLARE(1)    // Таймер сработал
#define EVENT_TIMER_STATE_CIRQ          EVENT_TIMER_STATE_DECLARE(2)    // Обработка таймера из прерывания
// Составные состояния таймера
#define EVENT_TIMER_STATE_USED          (EVENT_TIMER_STATE_ACTIVE | EVENT_TIMER_STATE_PENDING)

// Класс реализации событий
static struct event_t
{
    // Данные для програмных таймеров
    class timer_t : event_base_t
    {
        // Хранение состояния таймера
        typedef uint8_t state_t;
        
        // Софтовые таймеры
        struct slot_t
        {
            // Счетчики интервалов
            event_interval_t current, reload;
            // Обработчик события
            notify_t *handler;
            // Состояние
            state_t state;
            
            // Конструктор по умолчанию
            slot_t(void) : state(EVENT_TIMER_STATE_NONE)
            { }
        } slot[EVENT_TIMER_COUNT];
        // Последнее значение аппаратного счетчика
        event_timer_period_t counter;
        
        // Поиск свободного таймера с начала списка (критичные по времени срабатывания)
        uint8_t find_free(void) const
        {
            for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
                if (!(slot[i].state & EVENT_TIMER_STATE_USED))
                    return i;
            return EVENT_TIMER_NOT_FOUND;
        }

        // Поиск свободного таймера с конца списка
        uint8_t find_free_back(void) const
        {
            for (uint8_t i = EVENT_TIMER_COUNT; i > 0;)
                if (!(slot[--i].state & EVENT_TIMER_STATE_USED))
                    return i;
            return EVENT_TIMER_NOT_FOUND;
        }

        // Поиск таймера по обработчику
        uint8_t find_handler(const notify_t &handler) const
        {
            for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
                if (slot[i].handler == &handler)
                    return i;
            return EVENT_TIMER_NOT_FOUND;
        }
    protected:
        // Обработчик тиков таймеров
        virtual void notify_event(void)
        {
            // Сохранение состояния прерываний
            IRQ_CTX_SAVE();
            // Обработка тиков таймеров
            for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
                if (slot[i].state & EVENT_TIMER_STATE_PENDING)
                {
                    // Обработчка тика
                    slot[i].handler->notify_event();
                    // Снятие состояния ожидания обработки
                    IRQ_CTX_DISABLE();
                        slot[i].state &= ~EVENT_TIMER_STATE_PENDING;
                    IRQ_CTX_RESTORE();
                }
        }
    public:
        // Конструктор по умолчанию
        timer_t(void) : counter(0)
        { }

        // Обработчик прерывания аппаратного таймера
        INLINE_FORCED
        OPTIMIZE_SPEED
        void interrupt_handler(void)
        {
            bool event_gen = false;
            event_timer_period_t dx, dt, ccr = EVENT_TIMER_PERIOD_MAX;
            // Опрделеение разницы времени в тиках
            dx = (event_timer_period_t)TIM3->CNT;
            dx -= counter;
            counter += dx;
            // Обработка таймеров
            for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
            {
                // Если таймер выключен - пропуск
                if (!(slot[i].state & EVENT_TIMER_STATE_ACTIVE))
                    continue;
                if (slot[i].current <= dx)
                {
                    // Генерирование события
                    if (slot[i].state & EVENT_TIMER_STATE_CIRQ)
                        // ...прямо из прерывания
                        slot[i].handler->notify_event();
                    else
                    {
                        // ...в основной нити
                        event_gen = true;
                        slot[i].state |= EVENT_TIMER_STATE_PENDING;
                    }
                    if (slot[i].reload <= 0)
                    {
                        // Отключение
                        slot[i].state &= ~EVENT_TIMER_STATE_ACTIVE;
                        continue;
                    }
                    // Определяем сколько времени прошляпили
                    dt = dx - slot[i].current;
                    // Нормализация пропавшего времени до значения перезагрузки
                    while (dt >= slot[i].reload)
                        dt -= slot[i].reload;
                    // Обновление интервала значением перезагрузки
                    slot[i].current = slot[i].reload;
                    // Вычитаем нормализированное потерянное время
                    slot[i].current -= dt;
                }
                else
                    slot[i].current -= dx;
                // Определение времени следующего срабатывания аппаратного таймера
                if (ccr > slot[i].current)
                    ccr = slot[i].current;
            }
            // Рассчет времени следующего срабатывания
            if (ccr < EVENT_TIMER_PERIOD_MIN)
                ccr = EVENT_TIMER_PERIOD_MIN;
            // Установка регистра CCR1
            ccr_set(ccr);
            // Генерация события
            if (event_gen)
                event_add(this);
        }
        
        // Установка нового значения для регистра захвата/сравнения
        OPTIMIZE_SPEED
        void ccr_set(event_timer_period_t delta) const
        {
            delta += (event_timer_period_t)TIM3->CNT;
            // Установка регистра CCR1
            TIM3->CCR1 = delta;
            // Сброс флага прерывания
            TIM3->SR &= ~TIM_SR_CC1IF;                                          // Clear IRQ CC1 pending flag
        }

        // Старт программного таймера на указанное количество тиков
        void start(notify_t &handler, event_interval_t interval, event_timer_flag_t flags)
        {
            // Проверка аргументов
            if (interval <= 0)
                return;
            // Отключаем все прерывания
            IRQ_SAFE_ENTER();
                // Поиск по обработчику
                uint8_t i = find_handler(handler);
                if (i == EVENT_TIMER_NOT_FOUND)
                {
                    // Поиск свободного слота
                    i = (flags & EVENT_TIMER_FLAG_HEAD) ? find_free() : find_free_back();
                    // Когда слот не найден - это недопустимо
                    assert(i != EVENT_TIMER_NOT_FOUND);
                }
                // Установка слота
                    // Обработчик, интервал
                slot[i].current = interval;
                slot[i].handler = &handler;
                    // Состояние (без флага ожидания)
                slot[i].state |= EVENT_TIMER_STATE_ACTIVE;
                if (flags & EVENT_TIMER_FLAG_CIRQ)
                    slot[i].state |= EVENT_TIMER_STATE_CIRQ;
                else
                    slot[i].state &= ~EVENT_TIMER_STATE_CIRQ;
                    // Период
                slot[i].reload = (flags & EVENT_TIMER_FLAG_LOOP) ? interval : 0;
            // Восстановление прерываний
            IRQ_SAFE_LEAVE();
            // Форсирование срабатывания прерывания
            ccr_set(EVENT_TIMER_PERIOD_MIN);
        }

        // Стоп программного таймера (возвращает - нашелся ли таймер)
        INLINE_FORCED
        bool stop(const notify_t &handler)
        {
            bool result = false;
            // Отключаем все прерывания
            IRQ_SAFE_ENTER();
                // Поиск слота
                uint8_t i = find_handler(handler);
                if (i != EVENT_TIMER_NOT_FOUND)
                {
                    // Отключение
                    slot[i].state &= ~EVENT_TIMER_STATE_ACTIVE;
                    result = true;
                }
            // Восстановление прерываний
            IRQ_SAFE_LEAVE();
            return result;
        }
    } timer;
    
    // Списки общих событий
    class scope_t
    {
        list_template_t<event_base_t> list[2];
        list_template_t<event_base_t> *active;
    public:
        // Конструктор
        scope_t(void) : active(list)
        { }
        
        // Добавление события в очередь
        INLINE_FORCED
        void add(event_base_t &event)
        {
            // Добавление в начало списка событий
            IRQ_SAFE_ENTER();
                // Если событие уже в ожидании - выходим
                if (event.pending_set())
                    event.link(*active, LIST_SIDE_HEAD);
            IRQ_SAFE_LEAVE();
        }
        
        // Обработчка очереди событий
        INLINE_FORCED
        void execute(void)
        {
            // Проверяем, пустой ли список
            if (active->empty())
                return;
            uint8_t i;
            IRQ_SAFE_ENTER();
                // Определяем какой список был активен
                if (active == list + 0)
                    i = 0;
                else if (active == list + 1)
                    i = 1;
                // Выбираем активным списком другой
                active = list + (i ^ 1);
                assert(active->empty());
            IRQ_SAFE_LEAVE();
            // Обработка очереди
            do
            {
                // Получаем первый элемент списка
                event_base_t &event = *list[i].head();
                // Обработка события
                event.notify();
                // Удаляем из списка
                event.unlink();
                // Cбрасываем флаг ожидания
                IRQ_CTX_DISABLE();
                    event.pending_reset();
                IRQ_CTX_RESTORE();
            } while (!list[i].empty());
        }
    } scope;
} event;

void event_init(void)
{
    // Тактирование и сброс таймера
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;                                         // TIM3 clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM3RST;                                      // TIM3 reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM3RST;                                     // TIM3 unreset
    // Конфигурирование таймера
    TIM3->CCMR1 = TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1;                          // CC1 output, CC1 Fast off, CC1 preload off, CC1 mode Toggle (011)
    TIM3->PSC = FMCU_NORMAL_HZ / EVENT_TIMER_FREQUENCY_HZ - 1;                  // Prescaler (frequency)
    TIM3->ARR = EVENT_TIMER_PERIOD_RAW;                                         // Max period
    TIM3->DIER = TIM_DIER_CC1IE;                                                // CC1 IRQ enable
    // Конфигурирование прерываний
    nvic_irq_enable_set(TIM3_IRQn, true);                                       // TIM3 IRQ enable
    nvic_irq_priority_set(TIM3_IRQn, NVIC_IRQ_PRIORITY_HIGHEST);                // Middle TIM3 IRQ priority
    // Старт аппаратного таймера
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
