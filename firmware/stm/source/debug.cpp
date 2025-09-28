#include "io.h"
#include "mcu.h"
#include "nvic.h"
#include "event.h"
#include "debug.h"

// Используемый канал DMA
static DMA_Channel_TypeDef * const DEBUG_DMA_C7 = DMA1_Channel7;

// Размер буфера передачи 
constexpr auto DEBUG_BUFFER_SIZE = 512;

// Фаза заполняемого буфера
static bool debug_buffer_phase = false;

// Буферы передачи
static struct
{
    // Данные
    uint8_t data[DEBUG_BUFFER_SIZE];
    // Количество заполненных байт
    uint16_t size = 0;
} debug_buffer[2];

// Событие опроса состояния буфера
static event_t debug_buffer_pool_ev([]()
{
    if ((DEBUG_DMA_C7->CCR & DMA_CCR_EN) != 0)                                  // Check enable bit
        return;
    
    // Запуск DMA на текущем буфере
    auto &buffer = debug_buffer[debug_buffer_phase];
    if (buffer.size > 0)
    {
        mcu_dma_channel_setup_m(DEBUG_DMA_C7, buffer.data);
        DEBUG_DMA_C7->CNDTR = buffer.size;                                      // Transfer size
        DEBUG_DMA_C7->CCR |= DMA_CCR_EN;                                        // Channel enable
        buffer.size = 0;
    }
    
    // Смена фазы
    debug_buffer_phase = !debug_buffer_phase;
});

void debug_init()
{
    // Тактирование и сброс USART
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;                                       // USART2 clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_USART2RST;                                    // USART2 reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_USART2RST;                                   // USART2 unreset
    
    // Конфигурирование USART (115200 @ PCLK1 48 МГц)
    {
        // Конечная скорость передачи
        constexpr uint32_t BAUDRATE = 115200;
        
        // Отношение исходной частоты к скорости передачи
        constexpr auto RATIO = FPCLK1_HZ / (BAUDRATE * 16.0);
        // Целая часть
        constexpr auto RATIO_N = (uint32_t)RATIO;
        // Дробная часть
        constexpr auto RATIO_F = (uint32_t)((RATIO - RATIO_N) * 16.0 + 0.5);
        
        USART2->BRR = RATIO_F | RATIO_N << 4;                                   // Baudrate
    }

    // Конфигурирование DMA
    DMA1->IFCR |= DMA_IFCR_CTCIF7;                                              // Clear CTCIF
    // Канал 7
    mcu_dma_channel_setup_p(DEBUG_DMA_C7, USART2->DR);
    DEBUG_DMA_C7->CCR |= DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE;             // Memory to peripheral, Memory increment (8-bit), Transfer complete interrupt enable
    nvic_irq_enable(DMA1_Channel7_IRQn);

    // Запуск USART
    USART2->CR3 = USART_CR3_DMAT;                                               // DMA TX enable
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;                                  // TX on, 8-bit, Parity none, USART on
    
    // Возможно в буфере уже что то есть
    debug_buffer_pool_ev.raise();
}

void debug_pulse(void)
{
#ifdef DEBUG
    IO_PORT_SET(IO_RSV4);
    IO_PORT_RESET(IO_RSV4);
#endif
}

void debug_write(const void *source, size_t size)
{
    assert(source != NULL);
    
    // Заполняемый буфер
    auto &buffer = debug_buffer[debug_buffer_phase];
    
    // Количество доступных байт
    const auto avail = DEBUG_BUFFER_SIZE - buffer.size;
    
    // Усечение размера
    if (size > avail)
        size = avail;
    
    if (size <= 0)
        return;
    
    // Перенос
    memcpy(buffer.data + buffer.size, source, size);
    buffer.size += size;
    
    // Вызов опроса
    debug_buffer_pool_ev.raise();
}

IRQ_ROUTINE
void debug_interrupt_dma(void)
{
    DEBUG_DMA_C7->CCR &= ~DMA_CCR_EN;                                           // Channel disable
    DMA1->IFCR |= DMA_IFCR_CTCIF7;                                              // Clear CTCIF

    debug_buffer_pool_ev.raise();
}
