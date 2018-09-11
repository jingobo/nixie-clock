#ifndef __RTC_H
#define __RTC_H

#include "typedefs.h"
#include <datetime.h>

// Инициализация модуля
void rtc_init(void);
// Вывод частоты RTC /64
void rtc_clock_output(bool enabled);
// Получает текущую дату/время
void rtc_datetime_get(datetime_t &dest);

// Обработчик секундного прерывания
void rtc_interrupt_second(void);

#endif // __RTC_H
