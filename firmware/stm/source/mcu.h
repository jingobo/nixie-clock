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

// Инициализация модуля
void mcu_init(void);
// Импульс на выводе для отладки
void mcu_debug_pulse(void);
// Обработчик аварийной остановки приложения
__noreturn void mcu_halt(mcu_halt_reason_t reason);

// Обновение участка бит регистра
void mcu_reg_update_32(volatile uint32_t *reg, uint32_t value_bits, uint32_t valid_bits);
// Установка указателей каналу DMA (переферия <-> память)
void mcu_dma_channel_setup_pm(DMA_Channel_TypeDef *channel, volatile uint32_t &reg, const void *mem);

#endif // __MCU_H
