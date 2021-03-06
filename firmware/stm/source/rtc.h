﻿#ifndef __RTC_H
#define __RTC_H

#include "typedefs.h"
#include <datetime.h>
#include <callback.h>

// Инициализация модуля
void rtc_init(void);
// Вывод частоты RTC /64
void rtc_clock_output(bool enabled);
// Добавление обработчика секундного события 
void rtc_second_event_add(callback_list_item_t &callback);

// Получает текущую дату/время
void rtc_datetime_get(datetime_t &dest);
// Задает текущую дату/время
void rtc_datetime_set(const datetime_t &source);

// Обработчик секундного прерывания
void rtc_interrupt_second(void);

#endif // __RTC_H
