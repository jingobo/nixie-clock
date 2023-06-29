#include "mcu.h"
#include "nvic.h"

// Модули в которых есть прерывания
#include "esp.h"
#include "mcu.h"
#include "rtc.h"
#include "led.h"
#include "temp.h"
#include "timer.h"
#include "storage.h"

// Заглушка для обработчиков прерываний
IRQ_ROUTINE
static void nvic_interrupt_dummy(void)
{
    mcu_halt(MCU_HALT_REASON_IRQ);
}

// Обработчик Hard Fault
IRQ_ROUTINE
static void nvic_interrupt_hard(void)
{
    mcu_halt(MCU_HALT_REASON_SYS);
}

// Обработчик Memory Fault
IRQ_ROUTINE
static void nvic_interrupt_memory(void)
{
    mcu_halt(MCU_HALT_REASON_MEM);
}

// Обработчик Usage Fault
IRQ_ROUTINE
static void nvic_interrupt_usage(void)
{
    mcu_halt(MCU_HALT_REASON_USG);
}

// Обработчик Bus Fault
IRQ_ROUTINE
static void nvic_interrupt_bus(void)
{
    mcu_halt(MCU_HALT_REASON_BUS);
}

// Имя секции стека
#define NVIC_SECTION_STACK      "CSTACK"

// Обявление сегмента для sfe
SECTION_DECL(NVIC_SECTION_STACK)

// Структура описания стека и обработчиков прерываний
struct nvic_vtbl_t
{
    // Адрес базы стека
    void *stack_base;
    // Адреса обработчиков прерываний
    void (* irq_handler[58])(void);
};

extern "C"
{
    // Точка старта программы
    void __iar_program_start(void);

    // Имя не менять, это магическое значение для С-Spy
    __root const nvic_vtbl_t __vector_table @ ".intvec" =
    {
        __sfe(NVIC_SECTION_STACK),                  // Stack base
        {
            // Cortex-M3 Interrupts
            __iar_program_start,                    // Reset Handler
            nvic_interrupt_dummy,                   // NMI Handler
            nvic_interrupt_hard,                    // Hard Fault Handler
            nvic_interrupt_memory,                  // MPU Fault Handler
            nvic_interrupt_bus,                     // Bus Fault Handler
            nvic_interrupt_usage,                   // Usage Fault Handler
            NULL,                                   // Reserved
            NULL,                                   // Reserved
            NULL,                                   // Reserved
            NULL,                                   // Reserved
            nvic_interrupt_dummy,                   // SVCall Handler
            nvic_interrupt_dummy,                   // Debug Monitor Handler
            NULL,                                   // Reserved
            nvic_interrupt_dummy,                   // PendSV Handler
            mcu_interrupt_systick,                  // SysTick Handler
            
             // External Interrupts
            nvic_interrupt_dummy,                   // Window Watchdog
            nvic_interrupt_dummy,                   // PVD through EXTI Line detect
            nvic_interrupt_dummy,                   // Tamper
            rtc_interrupt_second,                   // RTC
            nvic_interrupt_dummy,                   // Flash
            nvic_interrupt_dummy,                   // RCC
            nvic_interrupt_dummy,                   // EXTI Line 0
            nvic_interrupt_dummy,                   // EXTI Line 1
            nvic_interrupt_dummy,                   // EXTI Line 2
            nvic_interrupt_dummy,                   // EXTI Line 3
            nvic_interrupt_dummy,                   // EXTI Line 4
            nvic_interrupt_dummy,                   // DMA1 Channel 1
            esp_interrupt_dma,                      // DMA1 Channel 2
            nvic_interrupt_dummy,                   // DMA1 Channel 3
            nvic_interrupt_dummy,                   // DMA1 Channel 4
            temp_interrupt_dma,                     // DMA1 Channel 5
            led_interrupt_dma,                      // DMA1 Channel 6
            nvic_interrupt_dummy,                   // DMA1 Channel 7
            nvic_interrupt_dummy,                   // ADC1 & ADC2
            nvic_interrupt_dummy,                   // USB High Priority or CAN1 TX
            nvic_interrupt_dummy,                   // USB Low  Priority or CAN1 RX0
            nvic_interrupt_dummy,                   // CAN1 RX1
            nvic_interrupt_dummy,                   // CAN1 SCE
            nvic_interrupt_dummy,                   // EXTI Line 9..5
            nvic_interrupt_dummy,                   // TIM1 Break
            nvic_interrupt_dummy,                   // TIM1 Update
            nvic_interrupt_dummy,                   // TIM1 Trigger and Commutation
            nvic_interrupt_dummy,                   // TIM1 Capture Compare
            nvic_interrupt_dummy,                   // TIM2
            timer_t::interrupt_htim,                // TIM3
            nvic_interrupt_dummy,                   // TIM4
            nvic_interrupt_dummy,                   // I2C1 Event
            nvic_interrupt_dummy,                   // I2C1 Error
            nvic_interrupt_dummy,                   // I2C2 Event
            nvic_interrupt_dummy,                   // I2C2 Error
            nvic_interrupt_dummy,                   // SPI1
            nvic_interrupt_dummy,                   // SPI2
            nvic_interrupt_dummy,                   // USART1
            nvic_interrupt_dummy,                   // USART2
            nvic_interrupt_dummy,                   // USART3
            nvic_interrupt_dummy,                   // EXTI Line 15..10
            nvic_interrupt_dummy,                   // RTC Alarm through EXTI Line
            nvic_interrupt_dummy,                   // USB Wakeup from suspend
        }
    };    
}

void nvic_init(void)
{
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP);
    
    // Таблица прерываний во ОЗУ
    #pragma data_alignment = 0x100
    static __root __no_init nvic_vtbl_t vtable_ram;
    
    // Инициализация таблицы прерываний в ОЗУ
    vtable_ram = __vector_table;
    SCB->VTOR = (uintptr_t)&vtable_ram;
}
