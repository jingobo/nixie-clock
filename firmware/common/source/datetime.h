#ifndef __DATETIME_H
#define __DATETIME_H

#include "common.h"

// Пределы для года
#define DATETIME_YEAR_MIN       1900
#define DATETIME_YEAR_MAX       2099
// Пределы для месяца
#define DATETIME_MONTH_MIN      1
#define DATETIME_MONTH_MAX      12
// Пределы для дней
#define DATETIME_DAY_MIN        1
#define DATETIME_DAY_MAX        31
// Пределы для часов
#define DATETIME_HOUR_MIN       0
#define DATETIME_HOUR_MAX       23
// Пределы для минут
#define DATETIME_MINUTE_MIN     0
#define DATETIME_MINUTE_MAX     59
// Пределы для секунд
#define DATETIME_SECOND_MIN     0
#define DATETIME_SECOND_MAX     59

ALIGN_FIELD_8
// Календарная дата/время
struct datetime_t
{
    // Дата
    uint16_t year;
    uint8_t month;
    uint8_t day;
    // Время
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    // Конструктор по умолчанию
    datetime_t();

    // Проверка на валидность
    bool check(void) const;
    // Получает, високосный ли год
    bool leap(void) const;
    
    // Получает кодичество дней в месяце
    uint8_t month_day_count(void) const;
    // Получает кодичество дней в месяце
    static uint8_t month_day_count(uint8_t month, bool leap);
};
ALIGN_FIELD_DEF

#endif // __DATETIME_H
