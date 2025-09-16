#include "rtc.h"
#include "mcu.h"
#include "nvic.h"
#include "event.h"
#include "system.h"
#include "storage.h"

// Локальное время
datetime_t rtc_time;
// День недели
uint8_t rtc_week_day = 5;
// Количество секунд с запуска
uint32_t rtc_uptime_seconds = 0;
// Частота кварца LSE
uint16_t rtc_lse_freq @ STORAGE_SECTION = RTC_LSE_FREQ_DEFAULT;

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

// Включает доступ на запись в бэкап домен 
static void rtc_backup_access_allow(void)
{
    PWR->CR |= PWR_CR_DBP;                                                      // Disable backup domain write protection
}

// Отключает доступ на запись в бэкап домен 
static void rtc_backup_access_deny(void)
{
    PWR->CR &= ~PWR_CR_DBP;                                                     // Enable backup domain write protection
}

// Применение частоты часового кварца
static void rtc_lse_freq_apply(void)
{
    assert(rtc_lse_freq > 0);
    
    IRQ_SAFE_ENTER();
        rtc_backup_access_allow();
            rtc_wait_operation_off();
                RTC->CRL |= RTC_CRL_CNF;                                        // Enter cfg mode
                    RTC->PRLL = rtc_lse_freq - 1;                               // Prescaler (low)
                    RTC->CRH = RTC_CRH_SECIE;                                   // Second IRQ enable
                RTC->CRL &= ~RTC_CRL_CNF;                                       // Leave cfg mode
            rtc_wait_operation_off();
        rtc_backup_access_deny();
    IRQ_SAFE_LEAVE();
}

void rtc_init(void)
{
    // Тактирование
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;                      // Power, backup interface clock enable
    
    IRQ_SAFE_ENTER();
        rtc_backup_access_allow();
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
        rtc_backup_access_deny();
    IRQ_SAFE_LEAVE();
    
    // Применение частоты часового кварца
    rtc_lse_freq_apply();
    
    // Прерывание
    nvic_irq_enable(RTC_IRQn);                                                  // RTC IRQ enable
    nvic_irq_priority_set(RTC_IRQn, NVIC_IRQ_PRIORITY_LOWEST);                  // Lowest RTC IRQ priority
}

void rtc_clock_output(bool enabled)
{
    IRQ_SAFE_ENTER();
        rtc_backup_access_allow();
            // Переконфигурирование PC13 на выход с альтернативной функцией производится аппаратно
            if (enabled)
                BKP->RTCCR |= BKP_RTCCR_CCO;                                    // Calibration clock output enable
            else
                BKP->RTCCR &= ~BKP_RTCCR_CCO;                                   // ...disable
        rtc_backup_access_deny();
    IRQ_SAFE_LEAVE();
}

// Список обработчиков секундных собюытий
static list_handler_t rtc_second_event_handlers;

// Обработчик события инкремента секунды
static event_t rtc_second_event([](void)
{
    // Инкремент рабочего времени
    rtc_uptime_seconds++;
    
    // Инкремент локального времени
    const auto day = rtc_time.day;
    rtc_time.inc_second();
    
    // Инкремент дня недели
    if (day != rtc_time.day)
        if (++rtc_week_day >= datetime_t::WDAY_COUNT)
            rtc_week_day = 0;
    
    // Вызов цепочки обработчиков
    rtc_second_event_handlers();
});

void rtc_second_event_add(list_handler_item_t &handler)
{
    handler.link(rtc_second_event_handlers);
}

void rtc_time_set(const datetime_t &value)
{
    rtc_time = value;
    rtc_week_day = value.day_week();
}

void rtc_lse_freq_set(uint16_t value)
{
    if (rtc_lse_freq == value)
        return;
    
    rtc_lse_freq = value;
    rtc_lse_freq_apply();
    storage_modified();
}

IRQ_ROUTINE
void rtc_interrupt_second(void)
{
    RTC->CRL &= ~RTC_CRL_SECF;                                                  // Clear IRQ pending flag
    rtc_second_event.raise();
}
