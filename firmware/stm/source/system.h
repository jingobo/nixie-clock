#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "typedefs.h"
#include <stm32f1xx.h>

/* Используемая переферия:
   mcu - RCC, SYSTICK
   rtc - RTC
   led - TIM1 (CH3), DMA1 (CH6)
   wdt - IWDG (2 Hz)
   timer - TIM3 (CH1)
   neon - TIM2 (CH3)
   nixie - TIM4 (CH2)
   esp - SPI1 (master), DMA1 (CH2, CH3)
   temp - USART1 (TX), DMA1 (CH4, CH5)
   light - I2C1
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

#endif // __SYSTEM_H
