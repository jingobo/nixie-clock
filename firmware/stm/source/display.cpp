#include "led.h"

#include "event.h"
#include "display.h"

// Обработчик обновления дисплея
static void display_refresh(void)
{
    led.refresh();
}

void display_init(void)
{
    // Запуск задания на обновление экрана
    //event_timer_start_hz(DELEGATE_PROC(display_refresh), HMI_FRAME_RATE, EVENT_TIMER_PRI_DEFAULT | EVENT_TIMER_FLAG_LOOP);
}
