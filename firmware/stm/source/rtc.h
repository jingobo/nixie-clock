#ifndef __RTC_H
#define __RTC_H

#include "typedefs.h"
#include <datetime.h>
#include <callback.h>

// Локальное время
extern datetime_t rtc_time;

// Инициализация модуля
void rtc_init(void);
// Вывод частоты RTC /64
void rtc_clock_output(bool enabled);
// Добавление обработчика секундного события 
void rtc_second_event_add(callback_list_item_t &callback);

// Обработчик секундного прерывания
void rtc_interrupt_second(void);

#endif // __RTC_H
