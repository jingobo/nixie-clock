#ifndef __MCU_H
#define __MCU_H

#include "system.h"

// ������� ���������
typedef enum
{
    // �� ������� ��������� HSE ��������� ��� �� �����
    MCU_HALT_REASON_RCC,
    // �� ������� ��������� LSE ��������� ��� �� �����
    MCU_HALT_REASON_RTC,
    
    // �� ������������� ����������
    MCU_HALT_REASON_IRQ,
    // ���������� Hard Fault
    MCU_HALT_REASON_SYS,
    // ���������� Memory Fault
    MCU_HALT_REASON_MEM,
    // ���������� Bus Fault
    MCU_HALT_REASON_BUS,
    // ���������� Usage Fault
    MCU_HALT_REASON_USG
} mcu_halt_reason_t;

// ������������� ������
void mcu_init(void);
// ������� �� ������ ��� �������
void mcu_debug_pulse(void);
// ���������� ��������� ��������� ����������
__noreturn void mcu_halt(mcu_halt_reason_t reason);

// ��������� ������� ��� ��������
void mcu_reg_update_32(volatile uint32_t *reg, uint32_t value_bits, uint32_t valid_bits);
// ��������� ���������� ������ DMA (��������� <-> ������)
void mcu_dma_channel_setup_pm(DMA_Channel_TypeDef *channel, volatile uint32_t &reg, const void *mem);

#endif // __MCU_H
