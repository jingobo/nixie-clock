#ifndef __IO_H
#define __IO_H

#include "system.h"

// Класс описания пина
class gpio_t
{
	// Параметры используемого пина
	uint8_t index;
	// Инверсия состояния
	bool inverse, state, opm;
	// Таймер для мигания
	os_timer_t blink_timer;

	// Обработчик таймера мигалки
	static void blink_timer_cb(void *arg);
public:
	// Инициализация структры пина
	void init(uint8_t index, uint32_t mux, uint8_t func, bool inverse);
	// Установка состояния включения пина
	void state_set(bool state);
	// Установка состояния мигания пина
	void blink_set(bool state);
	// Вспышка
	void flash(void);
};

// Доступные светодиоды
E_SYMBOL gpio_t IO_LED_YELLOW, IO_LED_GREEN;

// Инициализация модуля
void io_init(void);

#endif // __IO_H
