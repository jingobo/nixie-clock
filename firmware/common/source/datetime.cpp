#include "datetime.h"

// Конструктор по умолчанию
datetime_t::datetime_t()
{
    clear();
}

ROM void datetime_t::clear()
{
    year = DATETIME_YEAR_MIN;
    month = DATETIME_MONTH_MIN;
    day = DATETIME_DAY_MIN;
    hour = DATETIME_HOUR_MIN;
    minute = DATETIME_MINUTE_MIN;
    second = DATETIME_SECOND_MIN;
}

ROM bool datetime_t::check(void) const
{
    return year >= DATETIME_YEAR_MIN && year <= DATETIME_YEAR_MAX &&
           month >= DATETIME_MONTH_MIN && month <= DATETIME_MONTH_MAX &&
           day >= DATETIME_DAY_MIN && day <= month_day_count() &&
           hour >= DATETIME_HOUR_MIN && hour <= DATETIME_HOUR_MAX &&
           minute >= DATETIME_MINUTE_MIN && minute <= DATETIME_MINUTE_MAX &&
           second >= DATETIME_SECOND_MIN && second <= DATETIME_SECOND_MAX;
}

ROM bool datetime_t::leap(void) const
{
    return (!(year & 3)) && (year != DATETIME_YEAR_MIN);
}

// Получает кодичество дней в месяце
ROM uint8_t datetime_t::month_day_count(void) const
{
    return month_day_count(month, leap());
}

ROM uint8_t datetime_t::month_day_count(uint8_t month, bool leap)
{
    // Количество дней в месяцах
    static const uint8_t MONTH_DAYS[DATETIME_MONTH_MAX] = { 31,  0,  31,  30,  31,  30,  31,  31,  30,  31,  30,  31 };
    // Если февраль
    if (month == 2)
        return leap ? 29 : 28;
    // Остальные месяцы
    return MONTH_DAYS[month - 1];
}
