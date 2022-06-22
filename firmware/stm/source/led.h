#ifndef __LED_H
#define __LED_H

#include "hmi.h"
#include "xmath.h"

// Количество светодиодов
#define LED_COUNT       HMI_RANK_COUNT

// Класс модели фильтров светодиодов
class led_model_t : public hmi_model_t<hmi_rgb_t, LED_COUNT>
{ };

// Базовый класс фильтра
class led_filter_t : public led_model_t::filter_t
{
protected:
    // Приоритеты фильтров
    enum
    {
        // Освещенность
        PRIORITY_AMBIENT,
        // Плавное изменение цвета
        PRIORITY_SMOOTH,
    };
    
    // Основной конструктор
    led_filter_t(int8_t priority) : filter_t(priority)
    { }
};

// Инициализация модуля
void led_init(void);

// Обработчик DMA
void led_interrupt_dma(void);

#endif // __LED_H
