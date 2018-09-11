#ifndef __CLK_H
#define __CLK_H

#include "system.h"

// Источник тактирования вывода частоты
typedef enum
{
    CLK_MCO_SOURCE_NONE = RCC_CFGR_MCO_NOCLOCK,
    CLK_MCO_SOURCE_SYS = RCC_CFGR_MCO_SYSCLK,
    CLK_MCO_SOURCE_HSI = RCC_CFGR_MCO_HSI,
    CLK_MCO_SOURCE_HSE = RCC_CFGR_MCO_HSE,
    CLK_MCO_SOURCE_PLL = RCC_CFGR_MCO_PLLCLK_DIV2
} clk_mco_source_t;

// Целочисленный тип для периода системного таймера
typedef uint32_t clk_period_ms_t;

// Прототип функции опроса для операций с таймаутом
typedef bool (*clk_pool_handler_ptr)(void);

// Инициализация модуля
void clk_init(void);
// Вывод частоты на IO
void clk_mco_output(clk_mco_source_t source);
// Задержка в мС, не вызывать из прерываний
void clk_delay(clk_period_ms_t ms = 1);
// Обработка функций опроса, таймут в мС, не вызывать из прерываний
bool clk_pool(clk_pool_handler_ptr proc, clk_period_ms_t ms = 1);

// Обработчик прерывания системного таймера
void clk_interrupt_systick(void);

#endif // __CLK_H
