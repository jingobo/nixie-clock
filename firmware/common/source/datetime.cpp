#include "datetime.h"

void datetime_t::clear()
{
    year = DATETIME_YEAR_MIN;
    month = DATETIME_MONTH_MIN;
    day = DATETIME_DAY_MIN;
    hour = DATETIME_HOUR_MIN;
    minute = DATETIME_MINUTE_MIN;
    second = DATETIME_SECOND_MIN;
}

bool datetime_t::check(void) const
{
    return /*year >= DATETIME_YEAR_MIN &&*/ year <= DATETIME_YEAR_MAX &&
           month >= DATETIME_MONTH_MIN && month <= DATETIME_MONTH_MAX &&
           day >= DATETIME_DAY_MIN && day <= month_day_count() &&
           /*hour >= DATETIME_HOUR_MIN &&*/ hour <= DATETIME_HOUR_MAX &&
           /*minute >= DATETIME_MINUTE_MIN &&*/ minute <= DATETIME_MINUTE_MAX &&
           /*second >= DATETIME_SECOND_MIN &&*/ second <= DATETIME_SECOND_MAX;
}

uint8_t datetime_t::month_day_count(uint8_t month, bool leap)
{
    // Количество дней в месяцах
    static const uint8_t MONTH_DAYS[DATETIME_MONTH_MAX] = { 31,  0,  31,  30,  31,  30,  31,  31,  30,  31,  30,  31 };
    // Если февраль
    if (month == 2)
        return leap ? 29 : 28;
    // Остальные месяцы
    return MONTH_DAYS[month - 1];
}

void datetime_t::inc_second(void)
{
    if (++second <= DATETIME_SECOND_MAX)
        return;
    second = DATETIME_SECOND_MIN;
    // Инкремент минут
    if (++minute <= DATETIME_MINUTE_MAX)
        return;
    minute = DATETIME_MINUTE_MIN;
    // Инкремент часов
    if (++hour <= DATETIME_HOUR_MAX)
        return;
    hour = DATETIME_HOUR_MIN;
    // Инкремент дней
    if (++day <= month_day_count())
        return;
    day = DATETIME_DAY_MIN;
    // Инкремент месяцев
    if (++month <= DATETIME_MONTH_MAX)
        return;
    month = DATETIME_MONTH_MIN;
    // Инкремент лет
    if (++year <= DATETIME_YEAR_MAX)
        return;
    // Ничесе 0_o
    year = DATETIME_YEAR_MIN;
}
