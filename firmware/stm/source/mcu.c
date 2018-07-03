#include "io.h"
#include "mcu.h"
#include "system.h"

void mcu_init(void)
{
    // Сброс тактирования всей переферии
    RCC->AHBENR = 0;                                                            // Clock gate disable
    RCC->APB1ENR = 0;                                                           // Clock gate disable
    RCC->APB2ENR = 0;                                                           // Clock gate disable
    // Останов Watchdog при отладке
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;                                      // Disable watchdog on debug
}

__noreturn void mcu_halt(mcu_halt_reason_t reason)
{
    UNUSED(reason); // TODO: куданить сохранить причину
    INTERRUPTS_DISABLE();
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
    // Проверка указателя опущена
    uint32_t buffer = *reg;
    // Снимаем значащие биты
    buffer &= ~valid_bits;
    // Устанавливаем новые биты
    buffer |= value_bits;
    // Возвращаем в регистр
    *reg = buffer;
}
