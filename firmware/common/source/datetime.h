#ifndef __DATETIME_H
#define __DATETIME_H

#include "common.h"

// Пределы для года
#define DATETIME_YEAR_MIN       0
#define DATETIME_YEAR_MAX       99
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

// Смещение для значения года
#define DATETIME_YEAR_BASE      2000

// Календарная дата/время
struct datetime_t
{
    // Дата
    uint8_t year;
    uint8_t month;
    uint8_t day;
    // Время
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    // Конструктор по умолчанию
    datetime_t(void)
    {
        clear();
    }

    // Сброс полей
    void clear();
    
    // Проверка на валидность
    bool check(void) const;
    
    // Инкремент секунды
    void inc_second(void);
    // Инкремент минуты
    void inc_minute(void);
    // Инкремент часа
    void inc_hour(void);

    // Декремент секунды
    void dec_second(void);
    // Декремент минуты
    void dec_minute(void);
    // Декремент часа
    void dec_hour(void);
    
    // Смещение часа
    void shift_hour(int8_t dx);
    // Смещение минут
    void shift_minute(int8_t dx);
    
    // Получает кодичество дней в месяце
    static uint8_t month_day_count(uint8_t month, bool leap);

    // Получает, високосный ли год
    bool leap(void) const
    {
        return (year & 3) == 0;
    }
    
    // Получает кодичество дней в месяце
    uint8_t month_day_count(void) const
    {
        return month_day_count(month, leap());
    }
};

#endif // __DATETIME_H
