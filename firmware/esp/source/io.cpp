#include "io.h"

// Период мигания в мС
#define IO_LED_BLINK_DELAY          50

// Установка состояния вывода GPIO
#define IO_OUT_SET(pin, state)      GPIO_REG_WRITE(((state) ? GPIO_OUT_W1TS_ADDRESS : GPIO_OUT_W1TC_ADDRESS), (1L << (pin)))
// Включение/Отключение вывода GPIO
#define IO_ENA_SET(pin, state)      GPIO_REG_WRITE(((state) ? GPIO_ENABLE_W1TS_ADDRESS : GPIO_ENABLE_W1TC_ADDRESS), (1L << (pin)))

// Используемые светодиоды
io_led_t io_led_yellow, io_led_green;

// Обработчик таймера мигалки
void io_led_t::blink_timer_cb(void *arg)
{
    io_led_t *ref = (io_led_t *)arg;
    ref->state_set(!ref->state);
    if (ref->opm)
        ref->blink_set(false);
}

void io_led_t::init(uint8_t index, uint32_t mux, uint8_t func, bool inverse)
{
    this->opm = false;
    this->state = false;
    this->index = index;
    this->inverse = inverse;
    // Инициализация пина
    IO_ENA_SET(index, true);
    PIN_FUNC_SELECT(mux, func);
    // Инициализация таймера
    MEMORY_CLEAR(blink_timer);
    os_timer_setfn(&blink_timer, blink_timer_cb, this);
    // Установка состояния
    state_set(false);
}

void io_led_t::state_set(bool state)
{
    if (this->inverse)
        state = !state;
    this->state = state;
    IO_OUT_SET(index, (state ? 1 : 0));
}

void io_led_t::blink_set(bool state)
{
    this->opm = false;
    os_timer_disarm(&blink_timer);
    if (state)
        os_timer_arm(&blink_timer, IO_LED_BLINK_DELAY, true);
}

void io_led_t::flash(void)
{
    state_set(true);
    blink_set(true);
    this->opm = true;
}

void io_init(void)
{
    // Запрет вывода частоты кварца (я в шоке от китайцев)
    IO_OUT_SET(0, false);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    // Инициализация используемых светодиодов
    io_led_yellow.init(4, PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4, false);
    io_led_green.init(5, PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5, false);
}
