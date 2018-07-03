#ifndef __RTC_H
#define __RTC_H

#include "typedefs.h"

// Структуры хранения даты/времени
typedef struct
{
    // Дата
    uint16_t year;
    uint8_t month;
    uint8_t day;
    // Время
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_datetime_t;

// Инициализация модуля
void rtc_init(void);
// Вывод частоты RTC /64
void rtc_clock_output(bool enabled);

#endif // __RTC_H
