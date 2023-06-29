#ifndef __RTC_H
#define __RTC_H

#include <list.h>
#include <datetime.h>

// Локальное время
extern datetime_t rtc_time;

// Инициализация модуля
void rtc_init(void);
// Вывод частоты RTC /64
void rtc_clock_output(bool enabled);
// Добавление обработчика секундного события 
void rtc_second_event_add(list_handler_item_t &handler);

// Обработчик секундного прерывания
void rtc_interrupt_second(void);

#endif // __RTC_H
