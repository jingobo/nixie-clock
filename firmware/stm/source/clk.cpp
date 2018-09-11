#include "io.h"
#include "clk.h"
#include "mcu.h"
#include "wdt.h"
#include "nvic.h"

// Точность измерения времени SysTick [1...1000]
#define CLK_SYSTICK_DIVIDER        10

// Отсчет тиков системного таймера в мС
static volatile __no_init clk_period_ms_t clk_systick;

// Проверки готовности HSE
static bool clk_check_hse(void)
{
    return RCC->CR & RCC_CR_HSERDY;                                             // Check HSE ready flag
}

// Проверка готовности PLL
static bool clk_check_pll(void)
{
    return RCC->CR & RCC_CR_PLLRDY;                                             // Check PLL ready flag
}

// Проверка PLL как источкник HCLK
static bool clk_check_pll_hclk(void)
{
    return (RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL;                      // Check PLL as system clock
}

// Обновление системного таймера при смене тактирования
static void clk_systick_update(void)
{
    // Определяем частоту ядра
    uint32_t f_mcu = clk_check_pll_hclk() ? FMCU_NORMAL_MHZ : FMCU_STARTUP_MHZ;
    // Инициализация SysTick на 1 мС
    SysTick_Config(f_mcu * (1000 / CLK_SYSTICK_DIVIDER));
}

// Внутрення функция инициализация системы тактирования (пришлось разогнать до 96МГц)
INLINE_FORCED
static bool clk_init_internal(void)
{
    // Запуск HSE
    RCC->CR &= ~RCC_CR_HSEBYP;                                                  // HSE bypass disable
    RCC->CR |= RCC_CR_HSEON;                                                    // HSE enable
    // Ожидание запуска HSE, 100 мС
    if (!clk_pool(clk_check_hse, 100))                                          // Wait for HSE ready
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
    if (!clk_pool(clk_check_pll))                                               // Wait for PLL ready
        return true;
    mcu_reg_update_32(&FLASH->ACR, FLASH_ACR_LATENCY_1, FLASH_ACR_LATENCY);     // Flash latency 2 wait states
    // Переключение тактовой частоты
    mcu_reg_update_32(&RCC->CFGR, RCC_CFGR_SW_PLL, RCC_CFGR_SW);                // PLL select
    // Ожидание перехода на PLL, 1 мС
    if (!clk_pool(clk_check_pll_hclk))                                          // Wait for PLL ready
        return true;
    clk_systick_update();
    // Запуск CSS на HSE
    RCC->CR |= RCC_CR_CSSON;                                                    // CSS on HSE enable
    return false;
}

void clk_init(void)
{
    // Явно включать прерывание SysTick нельзя, оно немаскируемое
    clk_systick_update();
    nvic_irq_priority_set(SysTick_IRQn, NVIC_IRQ_PRIORITY_LOWEST);              // Lowest SysTick IRQ priority
    // Инициализация тактирования
    if (clk_init_internal())
        mcu_halt(MCU_HALT_REASON_RCC);
}

void clk_mco_output(clk_mco_source_t source)
{
    RCC->CFGR &= ~RCC_CFGR_MCO;
    switch (source)
    {
        case CLK_MCO_SOURCE_NONE:
            IO_PORT_CFG(IO_MCO, IO_MCO_OUTPUT_MODE);
            break;
        case CLK_MCO_SOURCE_SYS:
        case CLK_MCO_SOURCE_HSI:
        case CLK_MCO_SOURCE_HSE:
        case CLK_MCO_SOURCE_PLL:
            IO_PORT_CFG(IO_MCO, IO_MODE_OUTPUT_APP_50MHZ);
            RCC->CFGR |= source;
            break;
    }
}

// Шаблон для функции ожидания
#define CLK_DELAY_TEMPLATE(code)                                \
    assert(ms <= ((clk_period_ms_t)-1) / CLK_SYSTICK_DIVIDER);  \
    ms *= CLK_SYSTICK_DIVIDER;                                  \
    clk_period_ms_t old = clk_systick, cur;                     \
    do                                                          \
    {                                                           \
        code;                                                   \
        wdt_pulse();                                            \
        cur = clk_systick;                                      \
    } while (cur - old < ms)

INLINE_NEVER
OPTIMIZE_NONE
void clk_delay(clk_period_ms_t ms)
{
    CLK_DELAY_TEMPLATE(MACRO_EMPTY);
}

INLINE_NEVER
OPTIMIZE_NONE
bool clk_pool(clk_pool_handler_ptr proc, clk_period_ms_t ms)
{
    assert(proc != NULL);
    CLK_DELAY_TEMPLATE(if (proc()) return true);
    return false;
}

IRQ_ROUTINE
void clk_interrupt_systick(void)
{
    clk_systick++;
}
