#ifndef __MCU_H
#define __MCU_H

#include "system.h"

// Причина остановки
typedef enum
{
    // Не удалось запустить HSE осцилятор или он встал
    MCU_HALT_REASON_RCC,
    // Не удалось запустить LSE осцилятор или он встал
    MCU_HALT_REASON_RTC,
    // Ошибка при записи Fдash
    MCU_HALT_REASON_FLASH,
    
    // Не обработнанное прерывание
    MCU_HALT_REASON_IRQ,
    // Исключение Hard Fault
    MCU_HALT_REASON_SYS,
    // Исключение Memory Fault
    MCU_HALT_REASON_MEM,
    // Исключение Bus Fault
    MCU_HALT_REASON_BUS,
    // Исключение Usage Fault
    MCU_HALT_REASON_USG
} mcu_halt_reason_t;

// Источник тактирования вывода частоты
typedef enum
{
    MCU_MCO_SOURCE_NONE = RCC_CFGR_MCO_NOCLOCK,
    MCU_MCO_SOURCE_SYS = RCC_CFGR_MCO_SYSCLK,
    MCU_MCO_SOURCE_HSI = RCC_CFGR_MCO_HSI,
    MCU_MCO_SOURCE_HSE = RCC_CFGR_MCO_HSE,
    MCU_MCO_SOURCE_PLL = RCC_CFGR_MCO_PLLCLK_DIV2
} mcu_mco_source_t;

// Прототип функции опроса для операций с таймаутом
typedef bool (*mcu_pool_handler_ptr)(void);

// Инициализация модуля
void mcu_init(void);
// Вывод частоты на IO MCO
void clk_mco_output(mcu_mco_source_t source);
// Обработчик аварийной остановки приложения
RAM_IAR
__noreturn void mcu_halt(mcu_halt_reason_t reason);

// Получает текущее значение тиков в мС
uint32_t mcu_tick_get(void);
// Обработка функций опроса, таймут в мС, не вызывать из прерываний
bool mcu_pool_ms(mcu_pool_handler_ptr pool, uint32_t delay = 1);

// Обновение участка бит регистра
inline void mcu_reg_update_32(volatile uint32_t *reg, uint32_t value_bits, uint32_t valid_bits)
{
    // Проверка указателя опущена
    uint32_t buffer = *reg;
    // Снимаем значащие биты
    buffer &= ~valid_bits;
    // Устанавливаем новые биты
    buffer |= value_bits;
    // Возвращаем в регистр
    *reg = buffer;
}

// Установка указателя на память каналу DMA
inline void mcu_dma_channel_setup_m(DMA_Channel_TypeDef *channel, const void *mem)
{
    // Проверка аргументов
    assert(mem != NULL);
    assert(channel != NULL);
    
    WARNING_SUPPRESS(Pa039)
        channel->CMAR = (uint32_t)mem;                                          // Memory address
    WARNING_DEFAULT(Pa039)
}

// Установка указателя на периферию каналу DMA
inline void mcu_dma_channel_setup_p(DMA_Channel_TypeDef *channel, volatile uint32_t &reg)
{
    channel->CCR = 0;                                                           // Channel reset
    
    WARNING_SUPPRESS(Pa039)
        channel->CPAR = (uint32_t)&reg;                                         // Peripheral address
    WARNING_DEFAULT(Pa039)
}

// Установка указателей каналу DMA (переферия <-> память)
inline void mcu_dma_channel_setup_pm(DMA_Channel_TypeDef *channel, volatile uint32_t &reg, const void *mem)
{
    mcu_dma_channel_setup_p(channel, reg);
    mcu_dma_channel_setup_m(channel, mem);
}

// Обработчик прерывания системного таймера
void mcu_interrupt_systick(void);

#endif // __MCU_H
