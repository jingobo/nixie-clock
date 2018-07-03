#include "io.h"

// Период мигания в мС
#define GPIO_BLINK_DELAY 250

// Обработчик таймера мигалки
static void blink_timer_cb(void *arg)
{
	gpio_t *ref = (gpio_t *)arg;
	gpio_state_set(ref, !ref->state);
}

void gpio_setup(gpio_t *ref, uint8_t index, uint32_t mux, uint8_t func, bool inverse)
{
	if (!ref)
		return;
	ref->state = false;
	ref->index = index;
	ref->inverse = inverse;
	// Инициализация пина
	PIN_FUNC_SELECT(mux, func);
	// Инициализация таймера
	gpio_blink_set(ref, false);
	os_timer_setfn(&ref->blink_timer, blink_timer_cb, ref);
}

void gpio_state_set(gpio_t *ref, bool state)
{
	if (!ref)
		return;
	ref->state = state;
	if (ref->inverse)
		state = !state;
	GPIO_OUTPUT_SET(ref->index, state ? 1 : 0);
}

void gpio_blink_set(gpio_t *ref, bool state)
{
	if (!ref)
		return;
	os_timer_disarm(&ref->blink_timer);
	gpio_state_set(ref, false);
	if (state)
		os_timer_arm(&ref->blink_timer, GPIO_BLINK_DELAY, true);
}
