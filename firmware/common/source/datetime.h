#ifndef __DATETIME_H
#define __DATETIME_H

#include "common.h"

// Календарная дата/время
struct datetime_t
{
    // Пределы для года
    static constexpr const uint8_t YEAR_MIN = 0;
    static constexpr const uint8_t YEAR_MAX = 99;
    // Пределы для месяца
    static constexpr const uint8_t MONTH_MIN = 1;
    static constexpr const uint8_t MONTH_MAX = 12;
    // Пределы для дней
    static constexpr const uint8_t DAY_MIN = 1;
    static constexpr const uint8_t DAY_MAX = 31;
    // Пределы для дней недели
    static constexpr const uint8_t WDAY_MIN = 0;
    static constexpr const uint8_t WDAY_MAX = 6;
    // Пределы для часов
    static constexpr const uint8_t HOUR_MIN = 0;
    static constexpr const uint8_t HOUR_MAX = 23;
    // Пределы для минут
    static constexpr const uint8_t MINUTE_MIN = 0;
    static constexpr const uint8_t MINUTE_MAX = 59;
    // Пределы для секунд
    static constexpr const uint8_t SECOND_MIN = 0;
    static constexpr const uint8_t SECOND_MAX = 59;

    // Количество дней недели
    static constexpr const uint8_t WDAY_COUNT = 7;

    // Количество секунд в минуте
    static constexpr const uint32_t SECONDS_PER_MINUTE = 60; 
    // Количество минут в часе
    static constexpr const uint32_t MINUTES_PER_HOUR = 60; 
    // Количество часов в сутках
    static constexpr const uint32_t HOURS_PER_DAY = 24; 
    // Количество секунд в часе
    static constexpr const uint32_t SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
    // Количество секунд в сутках
    static constexpr const uint32_t SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY; 
    
    // Смещение для значения года
    static constexpr const uint16_t YEAR_BASE = 2000;

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
    // Проверка на пустое значение
    bool empty(void) const;
    
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
    
    // Получает день недели [0..6]
    uint8_t day_week(void) const;
    
    // Получает кодичество дней в месяце
    uint8_t month_day_count(void) const
    {
        return month_day_count(month, leap());
    }
    
    // Формирование даты из секунд с UTC базой
    static bool utc_from_seconds(uint64_t seconds, datetime_t &dest);
    // Формирование секунд с UTC базой
    uint64_t utc_to_seconds(void) const;

private:
    // Получает количество дней с UTC базой
    uint32_t utc_day_count(void) const;
};

#endif // __DATETIME_H
