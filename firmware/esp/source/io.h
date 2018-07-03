#ifndef __IO_H
#define __IO_H

#include "typedefs.h"

// Струткра описания пина, к полям не обращаться напрямую!
typedef struct
{
	// Параметры используемого пина
	uint8_t index;
	// Инверсия состояния
	bool inverse, state;
	// Таймер для мигания
	os_timer_t blink_timer;
} gpio_t;

// Инициализация структры пина
void gpio_setup(gpio_t *ref, uint8_t index, uint32_t mux, uint8_t func, bool inverse);
// Установка состояния включения пина
void gpio_state_set(gpio_t *ref, bool state);
// Установка состояния мигания пина
void gpio_blink_set(gpio_t *ref, bool state);

#endif // __IO_H
