﻿#include "io.h"
#include "mcu.h"
#include "wdt.h"
#include "nvic.h"

// Точность измерения времени SysTick [1...1000]
#define MCU_SYSTICK_DIVIDER        10

// Количество тиков системного таймера в мС
static volatile __no_init uint32_t mcu_systicks;

// Проверки готовности HSE
static bool mcu_check_hse(void)
{
    return RCC->CR & RCC_CR_HSERDY;                                             // Check HSE ready flag
}

// Проверка готовности PLL
static bool mcu_check_pll(void)
{
    return RCC->CR & RCC_CR_PLLRDY;                                             // Check PLL ready flag
}

// Проверка PLL как источкник HCLK
static bool mcu_check_pll_hclk(void)
{
    return (RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL;                      // Check PLL as system clock
}

// Обновление системного таймера при смене тактирования
static void mcu_systick_update(void)
{
    // Определяем частоту ядра
    uint32_t f_mcu = mcu_check_pll_hclk() ? FMCU_NORMAL_MHZ : FMCU_STARTUP_MHZ;
    // Инициализация SysTick на 1 мС
    SysTick_Config(f_mcu * (1000 / MCU_SYSTICK_DIVIDER));
}

// Внутрення функция инициализация системы тактирования (пришлось разогнать до 96МГц)
static bool mcu_init_rcc(void)
{
    // Запуск HSE
    RCC->CR &= ~RCC_CR_HSEBYP;                                                  // HSE bypass disable
    RCC->CR |= RCC_CR_HSEON;                                                    // HSE enable
    // Ожидание запуска HSE, 100 мС
    if (!mcu_pool_ms(mcu_check_hse, 100))                                       // Wait for HSE ready
        return true;
    // Конфигурирование высокочастотной части системы тактирования
    mcu_reg_update_32(&RCC->CFGR,                                               // HPRE /1, AHB /1, APB1 /2, APB2 /1, PLLSRC HSE, PLLXTPRE /1, PLL x12, USBPRE /1.5
        // Value
        RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1 |
        RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE_HSE | RCC_CFGR_PLLMULL12,
        // Mask
        RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_PLLSRC |
        RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL | RCC_CFGR_USBPRE);
    // Запуск PLL и ожидание стабилизации, 2 мС
    RCC->CR |= RCC_CR_PLLON;                                                    // PLL enable
    if (!mcu_pool_ms(mcu_check_pll))                                            // Wait for PLL ready
        return true;
    mcu_reg_update_32(&FLASH->ACR, FLASH_ACR_LATENCY_1, FLASH_ACR_LATENCY);     // Flash latency 2 wait states
    // Переключение тактовой частоты
    mcu_reg_update_32(&RCC->CFGR, RCC_CFGR_SW_PLL, RCC_CFGR_SW);                // PLL select
    // Ожидание перехода на PLL, 1 мС
    if (!mcu_pool_ms(mcu_check_pll_hclk))                                       // Wait for PLL ready
        return true;
    mcu_systick_update();
    // Запуск CSS на HSE
    RCC->CR |= RCC_CR_CSSON;                                                    // CSS on HSE enable
    return false;
}

void mcu_init(void)
{
    // Сброс тактирования всей переферии
    RCC->AHBENR = 0;                                                            // Clock gate disable
    RCC->APB1ENR = 0;                                                           // Clock gate disable
    RCC->APB2ENR = 0;                                                           // Clock gate disable
    // Останов Watchdog при отладке
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;                                      // Disable watchdog on debug
    // Тактирование DMA
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;                                           // DMA1 clock enable
    // Явно включать прерывание SysTick нельзя, оно немаскируемое
    mcu_systick_update();
    nvic_irq_priority_set(SysTick_IRQn, NVIC_IRQ_PRIORITY_LOWEST);              // Lowest SysTick IRQ priority
    // Инициализация тактирования
    if (mcu_init_rcc())
        mcu_halt(MCU_HALT_REASON_RCC);
}

__noreturn void mcu_halt(mcu_halt_reason_t reason)
{
    UNUSED(reason); // TODO: куданить сохранить причину
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
    // Проверка указателя опущена
    uint32_t buffer = *reg;
    // Снимаем значащие биты
    buffer &= ~valid_bits;
    // Устанавливаем новые биты
    buffer |= value_bits;
    // Возвращаем в регистр
    *reg = buffer;
}

void mcu_dma_channel_setup_pm(DMA_Channel_TypeDef *channel, volatile uint32_t &reg, const void *mem)
{
    // Проверка аргументов
    assert(channel != NULL && mem != NULL);
    // Инициализация канала
    channel->CCR = 0;                                                           // Channel reset
    WARNING_SUPPRESS(Pa039)
        channel->CPAR = (uint32_t)&reg;                                         // Peripheral address
        channel->CMAR = (uint32_t)mem;                                          // Memory address
    WARNING_DEFAULT(Pa039)
}

void mcu_mco_output(mcu_mco_source_t source)
{
    RCC->CFGR &= ~RCC_CFGR_MCO;
    switch (source)
    {
        case MCU_MCO_SOURCE_NONE:
            IO_PORT_CFG(IO_MCO, IO_MCO_OUTPUT_MODE);
            break;
        case MCU_MCO_SOURCE_SYS:
        case MCU_MCO_SOURCE_HSI:
        case MCU_MCO_SOURCE_HSE:
        case MCU_MCO_SOURCE_PLL:
            IO_PORT_CFG(IO_MCO, IO_MODE_OUTPUT_APP_50MHZ);
            RCC->CFGR |= source;
            break;
    }
}

// Шаблон для функции ожидания
#define MCU_DELAY_TEMPLATE(code)                            \
    assert(delay <= UINT32_MAX / MCU_SYSTICK_DIVIDER);      \
    delay *= MCU_SYSTICK_DIVIDER;                           \
    uint32_t old = mcu_systicks, cur;                       \
    do                                                      \
    {                                                       \
        code;                                               \
        wdt_pulse();                                        \
        cur = mcu_systicks;                                 \
    } while (cur - old < delay)

uint32_t mcu_tick_get(void)
{
    return mcu_systicks / MCU_SYSTICK_DIVIDER;
}

INLINE_NEVER
OPTIMIZE_NONE
void mcu_delay_ms(uint32_t delay)
{
    MCU_DELAY_TEMPLATE(MACRO_EMPTY);
}

INLINE_NEVER
OPTIMIZE_NONE
bool mcu_pool_ms(mcu_pool_handler_ptr proc, uint32_t delay)
{
    assert(proc != NULL);
    MCU_DELAY_TEMPLATE(
        if (proc()) 
            return true
    );
    return false;
}

IRQ_ROUTINE
void mcu_interrupt_systick(void)
{
    mcu_systicks++;
}
