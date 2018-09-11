#include "mcu.h"
#include "nvic.h"

// ������ ����� �����������
enum
{
    // 0 bits pre-emption priority, 4 bits subpriority
    NVIC_PRIORITYGROUP_0 = 7,
    // 1 bits pre-emption priority, 3 bits subpriority
    NVIC_PRIORITYGROUP_1 = 6,
    // 2 bits pre-emption priority, 2 bits subpriority
    NVIC_PRIORITYGROUP_2 = 5,
    // 3 bits pre-emption priority, 1 bits subpriority
    NVIC_PRIORITYGROUP_3 = 4,
    // 4 bits pre-emption priority, 0 bits subpriority
    NVIC_PRIORITYGROUP_4 = 3
};

// ������� ������������ ����� ����������� ����������
#define NVIC_PRIORITYGROUP      NVIC_PRIORITYGROUP_4

// �������� ��������� ����������� ����������
typedef void (* nvic_isr_ptr)(void);

// ��������� �������� ����� � ������������ ����������
struct nvic_vtbl_t
{
    // ����� ���� �����
    void *stack_base;
    // ������ ������������ ����������
    nvic_isr_ptr irq_handler[58];
};

void nvic_init(void)
{
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP);
}

OPTIMIZE_SIZE
void nvic_irq_priority_set(IRQn_Type irq, nvic_irq_priority_t priority)
{
    NVIC_SetPriority(irq, NVIC_EncodePriority(NVIC_PRIORITYGROUP, priority, NULL));
}

OPTIMIZE_SIZE
nvic_irq_priority_t nvic_irq_priority_get(IRQn_Type irq)
{
    uint32_t pri, sub;
    NVIC_DecodePriority(NVIC_GetPriority(irq), NVIC_PRIORITYGROUP, &pri, &sub);
    return (nvic_irq_priority_t)pri;
}

OPTIMIZE_SIZE
void nvic_irq_enable_set(IRQn_Type irq, bool state)
{
    assert(irq >= 0);
    if (state)
        NVIC_EnableIRQ(irq);
    else
        NVIC_DisableIRQ(irq);
}

// �������� ��� ������������ ����������
IRQ_ROUTINE
static void nvic_interrupt_dummy(void)
{
    mcu_halt(MCU_HALT_REASON_IRQ);
}

// ���������� Hard Fault
IRQ_ROUTINE
static void nvic_interrupt_hard(void)
{
    mcu_halt(MCU_HALT_REASON_SYS);
}

// ���������� Memory Fault
IRQ_ROUTINE
static void nvic_interrupt_memory(void)
{
    mcu_halt(MCU_HALT_REASON_MEM);
}

// ���������� Usage Fault
IRQ_ROUTINE
static void nvic_interrupt_usage(void)
{
    mcu_halt(MCU_HALT_REASON_USG);
}

// ���������� Bus Fault
IRQ_ROUTINE
static void nvic_interrupt_bus(void)
{
    mcu_halt(MCU_HALT_REASON_BUS);
}

// ����� ������ ���������
C_SYMBOL
void __iar_program_start(void);

// ������ � ������� ���� ����������
#include "esp.h"
#include "clk.h"
#include "rtc.h"
#include "led.h"
#include "tube.h"
#include "event.h"

// ��������� �������� ��� sfe
#pragma segment = SEGMENT_STACK
// ��� �� ������, ��� ���������� �������� ��� �-Spy
C_SYMBOL
__root const nvic_vtbl_t __vector_table @ SEGMENT_VTBL =
{
    __sfe(SEGMENT_STACK),                       // Stack base
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
        clk_interrupt_systick,                  // SysTick Handler
        
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
        nvic_interrupt_dummy,                   // DMA1 Channel 5
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
        event_interrupt_timer,                  // TIM3
        tube_interrupt_nixie_selcrst,           // TIM4
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