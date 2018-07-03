#include "event.h"
#include "nvic.h"
#include "system.h"

// Общее количество програмных таймеров
#define EVENT_TIMER_COUNT               10
// Количество микросекунд на тик
#define EVENT_TIMER_US_PER_TICK         4
// Аппаратная частота таймера событий
#define EVENT_TIMER_FREQUENCY_HZ        (MUL_M(1) / EVENT_TIMER_US_PER_TICK)

// Значение для индекса не найденного таймера
#define EVENT_TIMER_NOT_FOUND           UINT8_MAX

// Проверка количества (контракт)
#if EVENT_TIMER_COUNT >= EVENT_TIMER_NOT_FOUND
    #error software timer count too large
#endif

// Сырое значение периода для аппаратного таймера
#define EVENT_TIMER_PERIOD_RAW          ((event_t::timer_t::period_t)-1)
// Минимальный пероид в тиках
#define EVENT_TIMER_PERIOD_MIN          ((event_t::timer_t::period_t)2)
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

// Контроллер класса события
struct event_item_controller_t
{
    // Установка флага ожидания обработки
    static bool pending_set(event_item_t &item);
    // Сброс ожидания обработки
    static void pending_reset(event_item_t &item);
    // Обработка события
    static void execute(event_item_t &item);
private:
    // Закрытый конструктор
    event_item_controller_t(void)
    { }
};

// Класс реализации событий
struct event_t
{
    // Данные для програмных таймеров
    struct timer_t
    {
        // Тип данных для хранения пероида в тиках
        typedef uint16_t period_t;
    private:
        // Хранение состояния таймера
        typedef uint8_t state_t;
        
        // Софтовые таймеры
        struct slot_t
        {
            // Счетчики интервалов
            event_interval_t current, reload;
            // Обработчик события
            delegate_t delegate;
            // Состояние
            state_t state;
            
            // Конструктор по умолчанию
            slot_t(void) : state(EVENT_TIMER_STATE_NONE)
            { }
        } slot[EVENT_TIMER_COUNT];
        // Последнее значение аппаратного счетчика
        period_t counter;
        // Элемент списка событий
        event_item_t item;
        
        // Поиск свободного таймера с начала списка (критичные по времени срабатывания)
        uint8_t find_free(void) const;
        // Поиск свободного таймера с конца списка
        uint8_t find_free_back(void) const;
        // Поиск таймера по обработчику
        uint8_t find_delegate(const delegate_t &delegate) const;
    public:
        // Конструктор по умолчанию
        timer_t(void) : item(DELEGATE_METHOD(this, event_t::timer_t, tick_handler)), counter(0)
        { }
        
        // Обработчик тиков таймеров
        void tick_handler(void);
        // Обработчик прерывания аппаратного таймера
        void interrupt_handler(void);
        // Установка нового значения для регистра захвата/сравнения
        void ccr_set(period_t delta) const;
                
        // Старт программного таймера на указанное количество тиков
        void start(const delegate_t &delegate, event_interval_t interval, event_timer_flag_t flags);
        // Стоп программного таймера (возвращает - нашелся ли таймер)
        bool stop(const delegate_t &delegate);
    } timer;
    
    // Списки общих событий
    class scope_t
    {
        event_item_t *list[2];
        event_item_t **active;
    public:
        // Конструктор
        scope_t(void)
        {
            active = list;
            list[0] = list[1] = NULL;
        }
        
        // Добавление события в очередь
        void add(event_item_t &item);
        // Обработчка очереди событий (возвращает - нужно ли еще раз обработать)
        bool execute(void);
    } scope;    
};

// Экземпляр модуля
static event_t event;

event_item_t::event_item_t(delegate_t _delegate) : pending(false), delegate(_delegate) 
{ }

INLINE_FORCED
bool event_item_controller_t::pending_set(event_item_t &item)
{
    // Обработчик должен быть
    assert(item.delegate.ready());
    // Если уже установлен - выходим
    if (item.pending)
        return false;
    // Блокирование
    item.pending = true;
    return true;
}

INLINE_FORCED
void event_item_controller_t::pending_reset(event_item_t &item)
{
    item.pending = false;
}

INLINE_FORCED
void event_item_controller_t::execute(event_item_t &item)
{
    // Проверка состояния
    assert(item.delegate.ready());
    assert(item.pending);
    // Обработка события
    item.delegate();
}

INLINE_FORCED
void event_t::timer_t::tick_handler(void)
{
    INTERRUPTS_SAVE();
    // Обработка тиков таймеров
    for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
        if (slot[i].state & EVENT_TIMER_STATE_PENDING)
        {
            // Обработчка тика
            slot[i].delegate();
            // Снятие состояния ожидания обработки
            INTERRUPTS_DISABLE();
                slot[i].state &= ~EVENT_TIMER_STATE_PENDING;
            INTERRUPTS_RESTORE();
        }
}

OPTIMIZE_SPEED
void event_t::timer_t::ccr_set(period_t delta) const
{
    delta += (period_t)TIM3->CNT;
    // Установка регистра CCR1
    TIM3->CCR1 = delta;
    // Сброс флага прерывания
    TIM3->SR &= ~TIM_SR_CC1IF;                                                  // Clear IRQ CC1 pending flag
}

uint8_t event_t::timer_t::find_free(void) const
{
    for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
        if (!(slot[i].state & EVENT_TIMER_STATE_USED))
            return i;
    return EVENT_TIMER_NOT_FOUND;
}

uint8_t event_t::timer_t::find_free_back(void) const
{
    for (uint8_t i = EVENT_TIMER_COUNT; i > 0;)
        if (!(slot[--i].state & EVENT_TIMER_STATE_USED))
            return i;
    return EVENT_TIMER_NOT_FOUND;
}

uint8_t event_t::timer_t::find_delegate(const delegate_t &delegate) const
{
    for (uint8_t i = 0; i < EVENT_TIMER_COUNT; i++)
        if (slot[i].delegate == delegate)
            return i;
    return EVENT_TIMER_NOT_FOUND;
}

void event_t::timer_t::start(const delegate_t &delegate, event_interval_t interval, event_timer_flag_t flags)
{
    // Проверка аргументов
    assert(delegate.ready());
    if (interval <= 0)
        return;
    // Отключаем все прерывания
    INTERRUPT_SAFE_ENTER();
        // Поиск по обработчику
        uint8_t i = find_delegate(delegate);
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
        slot[i].delegate = delegate;
            // Состояние (без флага ожидания)
        slot[i].state |= EVENT_TIMER_STATE_ACTIVE;
        if (flags & EVENT_TIMER_FLAG_CIRQ)
            slot[i].state |= EVENT_TIMER_STATE_CIRQ;
        else
            slot[i].state &= ~EVENT_TIMER_STATE_CIRQ;
            // Период
        slot[i].reload = (flags & EVENT_TIMER_FLAG_LOOP) ? interval : 0;
    // Восстановление прерываний
    INTERRUPT_SAFE_LEAVE();
    // Форсирование срабатывания прерывания
    ccr_set(EVENT_TIMER_PERIOD_MIN);
}

INLINE_FORCED
bool event_t::timer_t::stop(const delegate_t &delegate)
{
    bool result = false;
    // Проверка аргументов
    assert(delegate.ready());
    // Отключаем все прерывания
    INTERRUPT_SAFE_ENTER();
        // Поиск слота
        uint8_t i = find_delegate(delegate);
        if (i != EVENT_TIMER_NOT_FOUND)
        {
            // Отключение
            slot[i].state &= ~EVENT_TIMER_STATE_ACTIVE;
            result = true;
        }
    // Восстановление прерываний
    INTERRUPT_SAFE_LEAVE();
    return result;
}

INLINE_FORCED
OPTIMIZE_SPEED
void event_t::timer_t::interrupt_handler(void)
{
    bool event_gen = false;
    period_t dx, dt, ccr = EVENT_TIMER_PERIOD_MAX;
    // Опрделеение разницы времени в тиках
    dx = (period_t)TIM3->CNT;
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
                slot[i].delegate();
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
        event_add(item);
}

INLINE_FORCED
void event_t::scope_t::add(event_item_t &item)
{
    // Добавление в начало списка событий
    INTERRUPT_SAFE_ENTER();
        // Если событие уже в ожидании - выходим
        if (event_item_controller_t::pending_set(item))
        {
            item.push(*active);
            *active = &item;
        }
    INTERRUPT_SAFE_LEAVE();
}

INLINE_FORCED
bool event_t::scope_t::execute(void)
{
    // Проверяем, пустой ли список
    if (*active == NULL)
        return false;
    uint8_t i;
    INTERRUPT_SAFE_ENTER();
        // Определяем какой список был активен
        if (*active == list[0])
            i = 0;
        else if (*active == list[1])
            i = 1;
        // Выбираем активным списком другой
        active = list + (i ^ 1);
        assert(*active == NULL);
    INTERRUPT_SAFE_LEAVE();
    // Обработка очереди
    event_item_t *prev, *item = list[i];
    while (item != NULL)
    {
        // Обработка события
        event_item_controller_t::execute(*item);
        // Удаляем из списка
        prev = item;
        item = (event_item_t *)item->remove();
        // Cбрасываем флаг ожидания
        INTERRUPTS_DISABLE();
            event_item_controller_t::pending_reset(*prev);
        INTERRUPTS_RESTORE();
    }
    list[i] = NULL;
    // Возвращаем, есть ли снова события на обработку
    return *active != NULL;
}

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

void event_add(event_item_t &item)
{
    event.scope.add(item);
}

bool event_execute(void)
{
    return event.scope.execute();
}

bool event_timer_stop(const delegate_t delegate)
{
    return event.timer.stop(delegate);
}

void event_timer_start_us(const delegate_t &delegate, event_interval_t us, event_timer_flag_t flags)
{
    event.timer.start(delegate, us / EVENT_TIMER_US_PER_TICK, flags);
}

void event_timer_start_hz(const delegate_t &delegate, float_t hz, event_timer_flag_t flags)
{
    assert(hz > 0.0f);
    assert(hz <= EVENT_TIMER_FREQUENCY_HZ);
    event.timer.start(delegate, (event_interval_t)(EVENT_TIMER_FREQUENCY_HZ / hz), flags);
}

IRQ_HANDLER
void event_interrupt_timer(void)
{
    event.timer.interrupt_handler();
}
