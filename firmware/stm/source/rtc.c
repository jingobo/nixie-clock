#include "rtc.h"
#include "clk.h"
#include "mcu.h"
#include "system.h"

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
INLINE_FORCED
static __noreturn void rtc_halt(void)
{
    mcu_halt(MCU_HALT_REASON_RTC);
}

// Ожидание завершения всех операций записи RTC
static void rtc_wait_operation_off(void)
{
    if (!clk_pool(rtc_check_rtoff))
        rtc_halt();
}

void rtc_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;                      // Power, backup interface clock enable
    BKP_ACCESS_ALLOW();
        // Конфигурирование LSE, RTC
        mcu_reg_update_32(&RCC->BDCR,                                           // LSE bypass off, LSE select for RTC, 
            RCC_BDCR_RTCSEL_LSE, 
            RCC_BDCR_LSEBYP | RCC_BDCR_RTCSEL);
        // Запуск LSE
        RCC->BDCR |= RCC_BDCR_LSEON;                                           // LSE enable
        // Ожидание запуска LSE, 6 сек
        if (!clk_pool(rtc_check_lse, 6000))
            rtc_halt();
        // Запуск RTC
        RCC->BDCR |= RCC_BDCR_RTCEN;                                            // RTC enable
        // Конфигурирование RTC
        rtc_wait_operation_off();
            RTC->CRL |= RTC_CRL_CNF;                                            // Enter cfg mode
                RTC->PRLL = 0x7FFF;                                             // /32768
                RTC->PRLH = 0;
                RTC->CNTL = 0;                                                  // Clear counter
                RTC->CNTH = 0;
            RTC->CRL &= ~RTC_CRL_CNF;                                           // Leave cfg mode
        rtc_wait_operation_off();
    BKP_ACCESS_DENY();
    // TODO: включить прерывание, приоритет низший, и считать секунды
}

void rtc_clock_output(bool enabled)
{
    // Переконфигурирование PC13 на выход с альтернативной функцией производится аппаратно
    BKP_ACCESS_ALLOW();
        if (enabled)
            BKP->RTCCR |= BKP_RTCCR_CCO;                                        // Calibration clock output enable
        else
            BKP->RTCCR &= ~BKP_RTCCR_CCO;                                       // ...disable
    BKP_ACCESS_DENY();
}
