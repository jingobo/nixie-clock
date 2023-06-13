#include "nvic.h"
#include "timer.h"
#include "system.h"

// Тип данных для хранения пероида в тиках
typedef uint16_t timer_period_t;

// Декларирование значения периода
#define TIMER_PERIOD_DECLARE(x)     ((timer_period_t)(x))
// Сырое значение периода для аппаратного таймера
#define TIMER_PERIOD_RAW            TIMER_PERIOD_DECLARE(-1)
// Минимальный пероид в тиках
#define TIMER_PERIOD_MIN            TIMER_PERIOD_DECLARE(2)
// Максимальный пероид в тиках
#define TIMER_PERIOD_MAX            (TIMER_PERIOD_RAW - TIMER_PERIOD_MIN)

// Списки таймеров
static struct
{
    // Запущенные
    list_template_t<timer_wrap_t> active;
    // Сработавшие
    list_template_t<timer_wrap_t> raised;
} timer_list;

// Последнее значение регистра CCR аппаратного таймера
static timer_period_t timer_ccr = 0;

// Событие вызова обработчиков сработавших таймеров
event_callback_t timer_base_t::call_event(call_event_cb);

// Установка нового значения для регистра захвата/сравнения
static void timer_ccr_inc(timer_period_t delta)
{
    delta += (timer_period_t)TIM3->CNT;
    // Установка регистра CCR1
    TIM3->CCR1 = delta;                                                         // Update CC1 value
    // Сброс флага прерывания
    TIM3->SR &= ~TIM_SR_CC1IF;                                                  // Clear IRQ CC1 pending flag
}

void timer_base_t::start(uint32_t interval, timer_flag_t flags)
{
    // Проверка аргументов
    assert(interval > 0);
    // Отключаем все прерывания
    IRQ_SAFE_ENTER();
        // Добавление с общий список
        if (active.unlinked())
            active.link(timer_list.active, (flags & TIMER_FLAG_HEAD) ?
                LIST_SIDE_HEAD :
                LIST_SIDE_LAST);
        // Периоды
        current = interval;
        reload = (flags & TIMER_FLAG_LOOP) ? interval : 0;
        // Вызов из прерывания?
        call_from_irq = (flags & TIMER_FLAG_CIRQ) > 0;
    // Восстановление прерываний
    IRQ_SAFE_LEAVE();
    // Форсирование срабатывания прерывания
    timer_ccr_inc(TIMER_PERIOD_MIN);
}

void timer_base_t::start_hz(float_t hz, timer_flag_t flags)
{
    assert(hz >= TIMER_HZ_MIN);
    assert(hz <= TIMER_HZ_MAX);
    start((uint32_t)(TIMER_FREQUENCY_HZ / hz), flags);
}

void timer_base_t::start_us(uint32_t us, timer_flag_t flags)
{
    assert(us >= TIMER_US_MIN);
    // assert(us <= TIMER_US_MAX);
    start(us / TIMER_US_PER_TICK, flags);
}

bool timer_base_t::stop(void)
{
    bool result = false;
    // Отключаем все прерывания
    IRQ_SAFE_ENTER();
        // Удаление из списка обработки
        result = active.linked();
        if (result)
            active.unlink();
    // Восстановление прерываний
    IRQ_SAFE_LEAVE();
    return result;
}

void timer_base_t::raise(void)
{
    // Отключаем все прерывания
    IRQ_SAFE_ENTER();
        if (!active.linked())
        {
            // Таймер не запущен
            IRQ_SAFE_LEAVE();
            return;
        }
        // Сброс времени до срабатывания
        current = 0;
    // Восстановление прерываний
    IRQ_SAFE_LEAVE();
    // Форсирование срабатывания прерывания
    timer_ccr_inc(TIMER_PERIOD_MIN);
}

void timer_base_t::call_event_cb(void)
{
    // Отключаем все прерывания
    IRQ_CTX_SAVE();
        IRQ_CTX_DISABLE();
        for (auto wrap = timer_list.raised.head(); wrap != NULL;)
        {
            auto &timer = wrap->timer;
            // Переход к следующему элементу
            wrap = (timer_wrap_t *)wrap->unlink();
            // Обработка тика таймера
            IRQ_CTX_RESTORE();
                timer.execute();
            IRQ_CTX_DISABLE();
        }
    // Восстановление прерываний
    IRQ_CTX_RESTORE();
}

IRQ_ROUTINE
void timer_base_t::interrupt_htim(void)
{
    bool event_raise = false;
    auto ccr = TIMER_PERIOD_MAX;
    // Опрделеение разницы времени в тиках
    auto dx = (timer_period_t)TIM3->CNT;
    dx -= timer_ccr;
    timer_ccr += dx;
    // Обработка таймеров
    for (auto wrap = timer_list.active.head(); wrap != NULL;)
    {
        auto &timer = wrap->timer;
        if (timer.current <= dx)
        {
            // Генерирование события
            if (timer.call_from_irq)
                // ...прямо из прерывания
                timer.execute();
            else
            {
                // ...в основной нити
                event_raise = true;
                if (timer.raised.unlinked())
                    timer.raised.link(timer_list.raised);
            }
            if (timer.reload <= 0)
            {
                // Отключение
                wrap = (timer_wrap_t *)wrap->unlink();
                continue;
            }
            // Определяем сколько времени прошляпили
            auto dt = dx - timer.current;
            // Нормализация пропавшего времени до значения перезагрузки
            while (dt >= timer.reload)
                dt -= timer.reload;
            // Обновление интервала значением перезагрузки
            timer.current = timer.reload;
            // Вычитаем нормализированное потерянное время
            timer.current -= dt;
        }
        else
            timer.current -= dx;
        // Определение времени следующего срабатывания аппаратного таймера
        if (ccr > timer.current)
            ccr = timer.current;
        // Переход к следующему таймеру
        wrap = LIST_ITEM_NEXT(wrap);
    }
    // Рассчет времени следующего срабатывания
    if (ccr < TIMER_PERIOD_MIN)
        ccr = TIMER_PERIOD_MIN;
    // Установка регистра CCR1
    timer_ccr_inc(ccr);
    // Генерация события
    if (event_raise)
        call_event.raise();
}

void timer_init(void)
{
    // Тактирование и сброс таймера
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;                                         // TIM3 clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM3RST;                                      // TIM3 reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM3RST;                                     // TIM3 unreset
    // Конфигурирование таймера
    TIM3->CCMR1 = TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1;                          // CC1 output, CC1 Fast off, CC1 preload off, CC1 mode Toggle (011)
    TIM3->PSC = FMCU_NORMAL_HZ / TIMER_FREQUENCY_HZ - 1;                        // Prescaler (frequency)
    TIM3->ARR = TIMER_PERIOD_RAW;                                               // Max period
    TIM3->DIER = TIM_DIER_CC1IE;                                                // CC1 IRQ enable
    // Конфигурирование прерываний
    nvic_irq_enable_set(TIM3_IRQn, true);                                       // TIM3 IRQ enable
    nvic_irq_priority_set(TIM3_IRQn, NVIC_IRQ_PRIORITY_HIGHEST);                // Middle TIM3 IRQ priority
    // Старт аппаратного таймера
    timer_ccr_inc(TIMER_PERIOD_MAX);
    TIM3->CR1 |= TIM_CR1_CEN;                                                   // TIM enable
}
