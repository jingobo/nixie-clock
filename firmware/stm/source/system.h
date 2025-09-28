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
   debug - USART2 (TX), DMA1 (CH7)
*/

// Частота ядра при старте
constexpr uint32_t FMCU_STARTUP_MHZ = 8;
constexpr uint32_t FMCU_STARTUP_KHZ = XK(FMCU_STARTUP_MHZ);
constexpr uint32_t FMCU_STARTUP_HZ = XM(FMCU_STARTUP_MHZ);

// Частота ядра после инициализации
constexpr uint32_t FMCU_NORMAL_MHZ = 96;
constexpr uint32_t FMCU_NORMAL_KHZ = XK(FMCU_NORMAL_MHZ);
constexpr uint32_t FMCU_NORMAL_HZ = XM(FMCU_NORMAL_MHZ);

// Частота шин после инициализации
constexpr uint32_t FPCLK1_HZ = FMCU_NORMAL_HZ / 2;
constexpr uint32_t FPCLK2_HZ = FMCU_NORMAL_HZ;

// Частота LSI [Гц]
constexpr uint32_t FLSI_KHZ = 40;
constexpr uint32_t FLSI_HZ = XK(FLSI_KHZ);

#endif // __SYSTEM_H
