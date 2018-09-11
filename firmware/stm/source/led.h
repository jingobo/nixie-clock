#ifndef __LED_H
#define __LED_H

#include "hmi.h"

// Количество светодиодов
#define LED_COUNT               HMI_RANK_COUNT

// Класс цепочки фильтров светодиодов
struct led_filter_chain_t : hmi_filter_chain_t<hmi_rgb_t, LED_COUNT>
{ };

// Класс фильтра коррекции гаммы для светодиодов
struct led_filter_gamma_t : hmi_filter_gamma_t<hmi_rgb_t, LED_COUNT>
{
protected:
    // События смены данных
    virtual void do_data_set(hmi_rank_t index, hmi_rgb_t &data);
};

// Цепочка фильтров для светодиодов
extern led_filter_chain_t led;
// Фильтр коррекции гаммы для светодиодов
extern led_filter_gamma_t led_filter_gamma;

// Инициализация модуля
void led_init(void);

// Обработчик DMA
void led_interrupt_dma(void);

#endif // __LED_H
