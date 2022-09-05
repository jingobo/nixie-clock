#include "mcu.h"
#include "led.h"
#include "nvic.h"
#include "screen.h"

// Alias
#define DMA1_C6                 DMA1_Channel6

// Количество цветовых каналов
#define LED_RGB_COM_COUNT       3
// Глубина в битах на канал
#define LED_RGB_BIT_DEPTH       8

// Период в тиках таймера
#define LED_TIMER_PERIOD        90
// Один период в тиках высокого состояния линии 
#define LED_HIBIT_PERIOD        (LED_TIMER_PERIOD * 41 / 125)
// Периоды для состояния бита
#define LED_LINE_BIT_LOW        (LED_HIBIT_PERIOD)
#define LED_LINE_BIT_HIGH       (LED_HIBIT_PERIOD * 2)

// Драйвер светодиодной подсветки
static class led_display_t : public led_model_t::display_t
{
    // Тип данных для всех компонент одного светодиода
    typedef uint8_t rgb_t[LED_RGB_COM_COUNT][LED_RGB_BIT_DEPTH];
    
    // Внутренний буфер данных для DMA
    struct 
    {
        // Для формирования паузы
        uint16_t reset[22];
        // Данные
        rgb_t data[LED_COUNT];
        // Для отчистки CCR и установки линии в 0
        uint8_t gap;
    } dma_buffer;
    
    // Было ли перемещение данных в буфер DMA
    bool uploaded = false;
protected:
    // Обработчик изменения данных
    virtual void data_changed(hmi_rank_t index, hmi_rgb_t &data) override final
    {
        uploaded = false;
    }
    
    // Обновление состояния светодиодов (возможна задержка выполнения до 0.25 мС)
    OPTIMIZE_SPEED
    virtual void refresh(void) override final
    {
        // Базовый метод
        display_t::refresh();
        
        // Если нужно перезалить данные в DMA
        if (uploaded)
            return;
        uploaded = true;
        
        // Ожидание остановки TIM
        WAIT_WHILE(TIM1->CR1 & TIM_CR1_CEN);
        // Подготовка смещения буфера DMA
        rgb_t *dest = dma_buffer.data + LED_COUNT;
        for (hmi_rank_t led = 0; led < LED_COUNT; led++)
        {
            rgb_t buf;
            auto rgb = get(led);
            // Коррекция гаммы
            rgb.correction(HMI_GAMMA_TABLE);
            // Формирование в буфер
            for (auto i = 0; i < LED_RGB_BIT_DEPTH; i++)
                for (auto j = 0; j < LED_RGB_COM_COUNT; j++)
                {
                    buf[j][i] = (rgb.grb[j] & 0x80) ? LED_LINE_BIT_HIGH : LED_LINE_BIT_LOW;
                    rgb.grb[j] <<= 1;
                }
            // Копирование в буфер DMA
            memcpy(--dest, buf, sizeof(rgb_t));
        }
        
        // Старт таймера
        TIM1->CR1 &= ~TIM_CR1_OPM;                                              // OPM disable
        TIM1->CR1 |= TIM_CR1_CEN;                                               // TIM enable
        // Старт DMA
        DMA1_C6->CNDTR = sizeof(dma_buffer);                                    // Transfer data size
        DMA1_C6->CCR |= DMA_CCR_EN;                                             // Channel enable        
    }
public:
    // Конструктор по умолчанию
    led_display_t(void)
    {
        MEMORY_CLEAR(dma_buffer);
    }

    // Получает указатель на буфер для DMA
    void * dma_pointer_get(void)
    {
        return &dma_buffer;
    }
} led_display;

void led_init(void)
{
    // Тактирование и сброс таймера
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;                                         // TIM1 clock enable
    RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST;                                      // TIM1 reset
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;                                     // TIM1 unreset
    // Конфигурирование таймера
    TIM1->CR1 = TIM_CR1_ARPE;                                                   // TIM disable, UEV on, OPM off, Up, CMS edge, Clock /1, ARR preloaded
    TIM1->DIER = TIM_DIER_CC3DE;                                                // CC3 DMA request enable
    TIM1->CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;        // CC3 output, CC3 Fast off, CC3 preloaded, CC3 mode PWM 1 (110)
    TIM1->CCER = TIM_CCER_CC3E;                                                 // CC3 output enable, CC3 Polarity high
    TIM1->ARR = LED_TIMER_PERIOD - 1;                                           // Period 800 kHz (1.25 uS)
    TIM1->BDTR = TIM_BDTR_MOE;                                                  // Main Output enable
    
    // Конфигурирование DMA
    DMA1->IFCR |= DMA_IFCR_CTCIF6;                                              // Clear CTCIF
    // Канал 6
    mcu_dma_channel_setup_pm(DMA1_C6,
        // В TIM CC3
        TIM1->CCR3, 
        // Из памяти
        led_display.dma_pointer_get());
    DMA1_C6->CCR |= DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 |              // Memory to peripheral, Memory increment (8-bit), Peripheral size 16-bit
                    DMA_CCR_PL | DMA_CCR_TCIE;                                  // Very high priority, Transfer complete interrupt enable
    // Включаем прерывание канала DMA
    nvic_irq_enable_set(DMA1_Channel6_IRQn, true);
    // Добавляем в цепочку драйвер
    screen.led.attach(led_display);
}

IRQ_ROUTINE
void led_interrupt_dma(void)
{
    TIM1->CR1 |= TIM_CR1_OPM;                                                   // OPM enable
    DMA1_C6->CCR &= ~DMA_CCR_EN;                                                // Channel disable
    DMA1->IFCR |= DMA_IFCR_CTCIF6;                                              // Clear CTCIF
}
