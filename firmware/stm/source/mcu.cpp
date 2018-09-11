#include "io.h"
#include "mcu.h"

void mcu_init(void)
{
    // ����� ������������ ���� ���������
    RCC->AHBENR = 0;                                                            // Clock gate disable
    RCC->APB1ENR = 0;                                                           // Clock gate disable
    RCC->APB2ENR = 0;                                                           // Clock gate disable
    // ������� Watchdog ��� �������
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;                                      // Disable watchdog on debug
    // ������������ DMA
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;                                           // DMA1 clock enable
}

__noreturn void mcu_halt(mcu_halt_reason_t reason)
{
    UNUSED(reason); // TODO: �������� ��������� �������
    IRQ_CTX_DISABLE();
    while (true)
    { }
}

void mcu_debug_pulse(void)
{
#ifdef DEBUG
    IO_PORT_SET(IO_RSV4);
    IO_PORT_RESET(IO_RSV4);
#endif
}

OPTIMIZE_SPEED
void mcu_reg_update_32(volatile uint32_t *reg, uint32_t value_bits, uint32_t valid_bits)
{
    // �������� ��������� �������
    uint32_t buffer = *reg;
    // ������� �������� ����
    buffer &= ~valid_bits;
    // ������������� ����� ����
    buffer |= value_bits;
    // ���������� � �������
    *reg = buffer;
}

void mcu_dma_channel_setup_pm(DMA_Channel_TypeDef *channel, volatile uint32_t &reg, const void *mem)
{
    // �������� ����������
    assert(channel != NULL && mem != NULL);
    // ������������� ������
    channel->CCR = 0;                                                           // Channel reset
    WARNING_SUPPRESS(Pa039)
        channel->CPAR = (uint32_t)&reg;                                         // Peripheral address
        channel->CMAR = (uint32_t)mem;                                          // Memory address
    WARNING_DEFAULT(Pa039)
}
