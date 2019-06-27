#include "rtc.h"
#include "timer.h"
#include "screen.h"

// Экран
screen_t screen;

// Таймер обновления экрана
static timer_callback_t screen_refresh_timer([](void)
{
    screen.refresh();
});

// Событие наступления секунды
static callback_list_item_t screen_second_event([](void)
{
    screen.second();
});

void screen_init(void)
{
    // Добавление обработчика секундного события
    rtc_second_event_add(screen_second_event);
    // Запуск задания на обновление экрана
    screen_refresh_timer.start_hz(HMI_FRAME_RATE, TIMER_PRI_HIGHEST | TIMER_FLAG_LOOP);
    // Обновление дисплея
    screen_refresh_timer.raise();
}
