﻿#include "mcu.h"
#include "led.h"
#include "nvic.h"
#include "screen.h"

// Используемый канал DMA
static DMA_Channel_TypeDef * const LED_DMA_C6 = DMA1_Channel6;

// Период в тиках таймера
constexpr const uint32_t LED_TIMER_PERIOD = 90;
// Один период в тиках высокого состояния линии 
constexpr const uint32_t LED_HIBIT_PERIOD = LED_TIMER_PERIOD * 41 / 125;
// Периоды для состояния бита
constexpr const uint32_t LED_LINE_BIT_LOW = LED_HIBIT_PERIOD;
constexpr const uint32_t LED_LINE_BIT_HIGH = LED_HIBIT_PERIOD * 2;

// Драйвер светодиодной подсветки
static class led_display_t : public led_model_t::display_t
{
    // Количество цветовых каналов
    static constexpr const uint8_t RGB_COM_COUNT = 3;
    // Глубина в битах на канал
    static constexpr const uint8_t RGB_BIT_DEPTH = 8;
    
    // Тип данных для всех компонент одного светодиода
    typedef uint8_t rgb_t[RGB_COM_COUNT][RGB_BIT_DEPTH];
    
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
    virtual void data_changed(hmi_rank_t index, led_data_t &data) override final
    {
        uploaded = false;
    }
    
    // Обновление состояния светодиодов (возможна задержка выполнения до 0.25 мС)
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
            auto rgb = in_get(led).rgb;
            // Коррекция гаммы
            rgb.correction(HMI_GAMMA_TABLE);
            // Формирование в буфер
            for (auto i = 0; i < RGB_BIT_DEPTH; i++)
                for (auto j = 0; j < RGB_COM_COUNT; j++)
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
        LED_DMA_C6->CNDTR = sizeof(dma_buffer);                                 // Transfer data size
        LED_DMA_C6->CCR |= DMA_CCR_EN;                                          // Channel enable        
    }
public:
    // Конструктор по умолчанию
    led_display_t(void)
    {
        memory_clear(&dma_buffer, sizeof(dma_buffer));
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
    mcu_dma_channel_setup_pm(LED_DMA_C6,
        // В TIM CC3
        TIM1->CCR3, 
        // Из памяти
        led_display.dma_pointer_get());
    LED_DMA_C6->CCR |= DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 |           // Memory to peripheral, Memory increment (8-bit), Peripheral size 16-bit
                    DMA_CCR_PL | DMA_CCR_TCIE;                                  // Very high priority, Transfer complete interrupt enable
    // Включаем прерывание канала DMA
    nvic_irq_enable(DMA1_Channel6_IRQn);
    // Добавляем в цепочку драйвер
    screen.led.attach(led_display);
}

IRQ_ROUTINE
void led_interrupt_dma(void)
{
    TIM1->CR1 |= TIM_CR1_OPM;                                                   // OPM enable
    LED_DMA_C6->CCR &= ~DMA_CCR_EN;                                             // Channel disable
    DMA1->IFCR |= DMA_IFCR_CTCIF6;                                              // Clear CTCIF
}

#include "random.h"

void led_source_t::color_last_next(void)
{
    switch (settings.source)
    {
        case DATA_SOURCE_CUR_SEQ:
            // Цвет из списка по очереди
            color_last = settings.rgb[rank_last_color++];
            if (rank_last_color >= LED_COUNT)
                rank_last_color = 0;
            break;

        case DATA_SOURCE_CUR_RANDOM:
            // Случайный цвет из списка
            {
                hmi_rgb_t color;
                hmi_rank_t count = 0;
                do
                {
                    color = settings.rgb[random_get(LED_COUNT)];
                } while (color_last == color && ++count < LED_COUNT * 2);
                color_last = color;
            }
            break;

        case DATA_SOURCE_ANY_RANDOM:
            // Случайный любой цвет
            color_last = led_random_color_get(hue_last);
            break;
            
        default:
            assert(false);
    }
}

void led_source_t::set_initial_ranks(void)
{
    // Установка цветов
    if (is_static_ranks())
    {
        // Настройки светодиодов
        const auto &rgb = settings.rgb;
        
        // Установка данных по разрядам
        for (hmi_rank_t i = 0; i < LED_COUNT; i++)
            out_set(i, rgb[i]);
        return;
    }
    
    switch (settings.effect)
    {
        case EFFECT_LEFT:
        case EFFECT_RIGHT:
        case EFFECT_IN:
        case EFFECT_OUT:
            rank_last = LED_COUNT_HALF - 1;
            // Нет опечатки break
        case EFFECT_FILL:
            color_last_next();
            for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                out_set(i, color_last);
            
            break;
            
        case EFFECT_FLASH:
            for (hmi_rank_t i = 0; i < LED_COUNT; i++)
            {
                rank_last = i;
                color_last_next();
                out_set(i, color_last);
            }
            break;
            
        default:
            assert(false);
    }
}

void led_source_t::refresh(void)
{
    // Базовый метод
    source_smoother_t::refresh();

    // Выходим сразу в режиме статичных разрядов
    if (is_static_ranks())
        return;
    
    // Обработка эффекта плавного перехода
    if (smoother_process())
        return;
    
    // Цвет
    switch (settings.effect)
    {
        // Подбор цвета всегда
        case EFFECT_FILL:
        case EFFECT_FLASH:
            {
                hmi_rank_t rank;
                do
                {
                    rank = (hmi_rank_t)random_get(LED_COUNT);
                } while (rank_last == rank);
                rank_last = rank;
            }
            color_last_next();
            break;
            
        // Подбор цвета через половину разрядов
        case EFFECT_LEFT:
        case EFFECT_RIGHT:
        case EFFECT_IN:
        case EFFECT_OUT:
            if (++rank_last < LED_COUNT_HALF)
                break;
            
            rank_last = 0;
            color_last_next();
            break;
        
        default:
            assert(false);
    }
                
    // Обработка эффекта
    switch (settings.effect)
    {
        case EFFECT_FILL:
            for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                smoother_start(i, color_last);
            break;
            
        case EFFECT_FLASH:
            // Установка трех разрядов
            process_rank_flash(rank_last - 1);
            process_rank_flash(rank_last + 1);
            process_rank_default(rank_last);
            break;
            
        case EFFECT_LEFT:
            for (hmi_rank_t i = 0; i < LED_COUNT - 1; i++)
                smoother_start(i, out_get(i + 1));
            process_rank_default(LED_COUNT - 1);
            break;
        
        case EFFECT_RIGHT:
            for (hmi_rank_t i = LED_COUNT - 1; i > 0; i--)
                smoother_start(i, out_get(i - 1));
            process_rank_default(0);
            break;
            
        case EFFECT_IN:
            for (hmi_rank_t i = LED_COUNT_HALF - 1; i > 0; i--)
                smoother_start(i, out_get(i - 1));
            process_rank_default(0);

            for (hmi_rank_t i = LED_COUNT_HALF; i < LED_COUNT - 1; i++)
                smoother_start(i, out_get(i + 1));
            process_rank_default(LED_COUNT - 1);
            break;
            
        case EFFECT_OUT:
            for (hmi_rank_t i = LED_COUNT - 1; i > LED_COUNT_HALF; i--)
                smoother_start(i, out_get(i - 1));
            process_rank_default(LED_COUNT_HALF);

            for (hmi_rank_t i = 0; i < LED_COUNT_HALF - 1; i++)
                smoother_start(i, out_get(i + 1));
            process_rank_default(LED_COUNT_HALF - 1);
            break;
            
        default:
            assert(false);
    }
}

void led_source_t::settings_apply(const settings_t *settings_old)
{
    // Признак начальной установки
    const auto initial = settings_old == NULL;
    
    // Признак 
    auto is_set_initial_ranks = initial;
    
    // Плавность
    smoother.time_set(settings.smooth);
    
    // Эффект и источник
    if (initial || 
        settings_old->effect != settings.effect || 
        settings_old->source != settings.source)
        is_set_initial_ranks = true;
    
    // Подсветка
    if (initial || rgb_equals(settings_old->rgb, settings.rgb))
        rgb_preview_timeout = 0;
    else
    {
        rgb_preview_timeout = 5;
        is_set_initial_ranks = true;
    }
    
    // Начальные разряды
    if (is_set_initial_ranks)
        set_initial_ranks();
}

hmi_rgb_t led_random_color_get(hmi_sat_t &hue_last, hmi_sat_t value)
{
    // Подбор оттенка
    constexpr const hmi_sat_t DELTA = 32;
    uint16_t hue = hue_last + DELTA + random_get(HMI_SAT_COUNT - DELTA * 2);
    while (hue > HMI_SAT_MAX)
        hue -= HMI_SAT_MAX;
    hue_last = (hmi_sat_t)hue;
    
    // Цвет в HSV
    const auto hsv = hmi_hsv_init(hue_last, (hmi_sat_t)random_range_get(220, HMI_SAT_MAX), value);
    // Конвертироование
    return hsv.to_rgb();
}
