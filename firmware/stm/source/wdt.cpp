#include "wdt.h"
#include "event.h"
#include "system.h"

// ��� ������ ��������� TODO:
#define WDT_UNUSED          MACRO_EMPTY

// ����� ��� ���������� IWDG
#define WDT_KEY_CONFIG      0x5555
#define WDT_KEY_RELOAD      0xAAAA
#define WDT_KEY_START       0xCCCC
// ������ ������� � ��
#define WDT_PERIOD_HZ       2

void wdt_init(void)
{
#ifndef WDT_UNUSED
    // ���������������� IWDG
    IWDG->KR = WDT_KEY_CONFIG;                                                  // Enable write access
        IWDG->PR = IWDG_PR_PR_0 | IWDG_PR_PR_1;                                 // Prescaler /32
        IWDG->RLR = FLSI_HZ / WDT_PERIOD_HZ / 32 - 1;                           // Reload value
    IWDG->KR = WDT_KEY_START;                                                   // Enable watchdog
    IWDG->KR = WDT_KEY_RELOAD;                                                  // Reload period
#endif
    // ����������� ������ �� ����� ������� (� ��� ���� ���� ��� ������� �������)
    //event_timer_start_hz(DELEGATE_PROC(wdt_pulse), WDT_PERIOD_HZ * 2, EVENT_TIMER_PRI_DEFAULT | EVENT_TIMER_FLAG_LOOP); TODO:
}

void wdt_pulse(void)
{
#ifndef WDT_UNUSED
    IWDG->KR = WDT_KEY_RELOAD;                                                  // Reload period
#endif
}

void wdt_stop(void)
{
#ifndef WDT_UNUSED
    IWDG->KR = WDT_KEY_CONFIG;                                                  // Enable write access
#endif
}
