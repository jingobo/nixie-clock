#include "rtc.h"
#include "mcu.h"
#include "nvic.h"
#include "event.h"
#include "system.h"

// Включает/Отключает доступ на запись в бэкап домен 
#define RTC_BKP_ACCESS_ALLOW()      PWR->CR |= PWR_CR_DBP                       // Disable backup domain write protection
#define RTC_BKP_ACCESS_DENY()       PWR->CR &= ~PWR_CR_DBP                      // Enable backup domain write protection

// Хранит локальное время
/*__no_init*/ static datetime_t rtc_time;

// Проверка работы LSE
static bool rtc_check_lse(void)
{
    return (RCC->BDCR & RCC_BDCR_LSERDY) == RCC_BDCR_LSERDY;                    // Check LSE ready flag
}

// Проверка флага завершения операций записи RTC
static bool rtc_check_rtoff(void)
{
    return (RTC->CRL & RTC_CRL_RTOFF) == RTC_CRL_RTOFF;                         // Check that RTC operation OFF
}

// Остановка приложения по причине сбоя RTC
__noreturn static void rtc_halt(void)
{
    mcu_halt(MCU_HALT_REASON_RTC);
}

// Ожидание завершения всех операций записи RTC
static void rtc_wait_operation_off(void)
{
    if (!mcu_pool_ms(rtc_check_rtoff))
        rtc_halt();
}

// Начальная нормализация значения частоты для регистра RTC_PRL
#define RTC_PRL(f)      ((f) - 1)
// Конечное усечение значения для части регистра RTC_PRL
#define RTC_PRLS(v)     ((uint16_t)(v))
// Расчет значения регистра RTC_PRLL от заданной частоты
#define RTC_PRLL(f)     RTC_PRLS((RTC_PRL(f) & 0xFFFF))
// Расчет значения регистра RTC_PRLР от заданной частоты
#define RTC_PRLH(f)     RTC_PRLS((RTC_PRL(f) >> 16))

void rtc_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;                      // Power, backup interface clock enable
    IRQ_SAFE_ENTER();
        RTC_BKP_ACCESS_ALLOW();
            // Конфигурирование LSE, RTC
            mcu_reg_update_32(&RCC->BDCR,                                       // LSE bypass off, LSE select for RTC, 
                RCC_BDCR_RTCSEL_LSE, 
                RCC_BDCR_LSEBYP | RCC_BDCR_RTCSEL);
            // Запуск LSE
            RCC->BDCR |= RCC_BDCR_LSEON;                                        // LSE enable
            // Ожидание запуска LSE, 6 сек
            if (!mcu_pool_ms(rtc_check_lse, 6000))
                rtc_halt();
            // Запуск RTC
            RCC->BDCR |= RCC_BDCR_RTCEN;                                        // RTC enable
            // Конфигурирование RTC
            rtc_wait_operation_off();
                RTC->CRL |= RTC_CRL_CNF;                                        // Enter cfg mode
                    // Реальная частота LSE
                    const auto FLSE = 32773;
                    RTC->PRLL = RTC_PRLL(FLSE);                                 // Prescaler (low)
                    RTC->PRLH = RTC_PRLH(FLSE);                                 // Prescaler (high)
                    RTC->CNTL = 0;                                              // Clear counter
                    RTC->CNTH = 0;
                    RTC->CRH = RTC_CRH_SECIE;                                   // Second IRQ enable
                RTC->CRL &= ~RTC_CRL_CNF;                                       // Leave cfg mode
            rtc_wait_operation_off();
        RTC_BKP_ACCESS_DENY();
    IRQ_SAFE_LEAVE();
    // Прерывание
    nvic_irq_enable_set(RTC_IRQn, true);                                        // RTC IRQ enable
    nvic_irq_priority_set(RTC_IRQn, NVIC_IRQ_PRIORITY_LOWEST);                  // Lowest RTC IRQ priority
}

void rtc_clock_output(bool enabled)
{
    IRQ_SAFE_ENTER();
        RTC_BKP_ACCESS_ALLOW();
            // Переконфигурирование PC13 на выход с альтернативной функцией производится аппаратно
            if (enabled)
                BKP->RTCCR |= BKP_RTCCR_CCO;                                    // Calibration clock output enable
            else
                BKP->RTCCR &= ~BKP_RTCCR_CCO;                                   // ...disable
        RTC_BKP_ACCESS_DENY();
    IRQ_SAFE_LEAVE();
}

void rtc_datetime_get(datetime_t &dest)
{
    dest = rtc_time;
}

void rtc_datetime_set(const datetime_t &source)
{
    assert(source.check());
    rtc_time = source;
}

// Список обработчиков секундных собюытий
static callback_list_t rtc_second_event_callbacks;

// Обработчик события инкремента секунды
static event_callback_t rtc_second_event([](void)
{
    // Инкремент секунды
    rtc_time.inc_second();
    // Вызов цепочки обработчиков
    rtc_second_event_callbacks();
});

void rtc_second_event_add(callback_list_item_t &callback)
{
    callback.link(rtc_second_event_callbacks);
}

IRQ_ROUTINE
void rtc_interrupt_second(void)
{
    RTC->CRL &= ~RTC_CRL_SECF;                                                  // Clear IRQ pending flag
    rtc_second_event.raise();
}
