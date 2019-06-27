#ifndef __LED_H
#define __LED_H

#include "hmi.h"

// Количество светодиодов
#define LED_COUNT       HMI_RANK_COUNT

// Класс модели фильтров светодиодов
class led_model_t : public hmi_model_t<hmi_rgb_t, LED_COUNT>
{ };

// Базовый класс фильтров для светодиодов
class led_filter_t : public led_model_t::filter_t
{
protected:
    // Основой конструктор
    led_filter_t(hmi_filter_purpose_t purpose) : filter_t(purpose)
    { }
};

// Инициализация модуля
void led_init(void);
// Обработчик DMA
void led_interrupt_dma(void);

#endif // __LED_H
