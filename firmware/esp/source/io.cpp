#include "io.h"

// Период мигания в мС
#define GPIO_BLINK_DELAY    50

gpio_t IO_LED_YELLOW, IO_LED_GREEN;

// Обработчик таймера мигалки
ROM void gpio_t::blink_timer_cb(void *arg)
{
	gpio_t *ref = (gpio_t *)arg;
	ref->state_set(!ref->state);
	if (ref->opm)
	    ref->blink_set(false);
}

ROM void gpio_t::init(uint8_t index, uint32_t mux, uint8_t func, bool inverse)
{
    this->opm = false;
	this->state = false;
	this->index = index;
	this->inverse = inverse;
	// Инициализация пина
    GPIO_ENA_SET(index, true);
	PIN_FUNC_SELECT(mux, func);
	// Инициализация таймера
	MEMORY_CLEAR(blink_timer);
	os_timer_setfn(&blink_timer, blink_timer_cb, this);
	// Установка состояния
	state_set(false);
}

ROM void gpio_t::state_set(bool state)
{
    if (this->inverse)
        state = !state;
	this->state = state;
	GPIO_OUT_SET(index, (state ? 1 : 0));
}

ROM void gpio_t::blink_set(bool state)
{
    this->opm = false;
	os_timer_disarm(&blink_timer);
	if (state)
		os_timer_arm(&blink_timer, GPIO_BLINK_DELAY, true);
}

ROM void gpio_t::flash(void)
{
    state_set(true);
    blink_set(true);
    this->opm = true;
}

ROM void io_init_entry(void)
{
    // Запрет вывода частоты кварца (я в шоке от китайцев)
    GPIO_OUT_SET(0, false);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
}

ROM void io_init(void)
{
    IO_LED_YELLOW.init(4, PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4, false);
    IO_LED_GREEN.init(5, PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5, false);
}
