#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "typedefs.h"
#include <stm32f1xx.h>

/* Используемая переферия:
   rtc - RTC
   led - TIM1 (CH3), DMA1 (CH6)
   wdt - IWDG (2 Hz)
   event - TIM3 (CH1)
   tube - TIM4 (CH2), TIM2 (CH3)
   esp - SPI1 (master), DMA1 (CH2, CH3)
*/

// Частота ядра при старте
#define FMCU_STARTUP_MHZ    8
#define FMCU_STARTUP_KHZ    XK(FMCU_STARTUP_MHZ)
#define FMCU_STARTUP_HZ     XM(FMCU_STARTUP_MHZ)
// Частота ядра после инициализации
#define FMCU_NORMAL_MHZ     96
#define FMCU_NORMAL_KHZ     XK(FMCU_NORMAL_MHZ)
#define FMCU_NORMAL_HZ      XM(FMCU_NORMAL_MHZ)

// Частота LSI [Гц]
#define FLSI_KHZ            40
#define FLSI_HZ             XK(FLSI_KHZ)

// Имя сегмента стека
#define SEGMENT_STACK       "CSTACK"
// Имя сегмента таблицы векторов прерываний
#define SEGMENT_VTBL        ".intvec"

// Включает/Отключает доступ на запись в бэкап домен 
#define BKP_ACCESS_ALLOW()  PWR->CR |= PWR_CR_DBP                               // Disable backup domain write protection
#define BKP_ACCESS_DENY()   PWR->CR &= ~PWR_CR_DBP;                             // Enable backup domain write protection

#endif // __SYSTEM_H
