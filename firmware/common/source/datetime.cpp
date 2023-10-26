#include "datetime.h"

void datetime_t::clear()
{
    year = YEAR_MIN;
    month = MONTH_MIN;
    day = DAY_MIN;
    hour = HOUR_MIN;
    minute = MINUTE_MIN;
    second = SECOND_MIN;
}

bool datetime_t::check(void) const
{
    return /*year >= YEAR_MIN &&*/ year <= YEAR_MAX &&
           month >= MONTH_MIN && month <= MONTH_MAX &&
           day >= DAY_MIN && day <= month_day_count() &&
           /*hour >= HOUR_MIN &&*/ hour <= HOUR_MAX &&
           /*minute >= MINUTE_MIN &&*/ minute <= MINUTE_MAX &&
           /*second >= SECOND_MIN &&*/ second <= SECOND_MAX;
}

bool datetime_t::empty(void) const
{
    return year == 0 && month == 0 && day == 0 &&
           hour == 0 && minute == 0 && second == 0;
}

uint8_t datetime_t::month_day_count(uint8_t month, bool leap)
{
    // Количество дней в месяцах
    static const uint8_t MONTH_DAYS[MONTH_MAX] = { 31,  0,  31,  30,  31,  30,  31,  31,  30,  31,  30,  31 };
    // Если февраль
    if (month == 2)
        return leap ? 29 : 28;
    // Остальные месяцы
    return MONTH_DAYS[month - 1];
}

void datetime_t::inc_second(void)
{
    if (++second <= SECOND_MAX)
        return;
    second = SECOND_MIN;
    inc_minute();
}

void datetime_t::inc_minute(void)
{
    if (++minute <= MINUTE_MAX)
        return;
    minute = MINUTE_MIN;
    inc_hour();
}

void datetime_t::inc_hour(void)
{
    if (++hour <= HOUR_MAX)
        return;
    hour = HOUR_MIN;
    // Инкремент дней
    if (++day <= month_day_count())
        return;
    day = DAY_MIN;
    // Инкремент месяцев
    if (++month <= MONTH_MAX)
        return;
    month = MONTH_MIN;
    // Инкремент лет
    if (++year <= YEAR_MAX)
        return;
    // Ничесе 0_o
    year = YEAR_MIN;
}

void datetime_t::dec_second(void)
{
    if (second > SECOND_MIN)
    {
        second--;
        return;
    }
    second = SECOND_MAX;
    dec_minute();
}

void datetime_t::dec_minute(void)
{
    if (minute > MINUTE_MIN)
    {
        minute--;
        return;
    }
    minute = MINUTE_MAX;
    dec_hour();
}

void datetime_t::dec_hour(void)
{
    if (hour > HOUR_MIN)
    {
        hour--;
        return;
    }
    hour = HOUR_MAX;
    // Декремент дней
    if (day > MINUTE_MIN)
    {
        day--;
        return;
    }
    // Декремент месяцев
    if (month > MONTH_MIN)
        month--;
    else
    {
        month = MONTH_MAX;
        // Декремент лет
        if (year > YEAR_MIN)
            year--;
        else
            // Ого 0_o
            year = YEAR_MAX;
    }
    day = month_day_count();
}

void datetime_t::shift_hour(int8_t dx)
{
    while (dx != 0)
        if (dx > 0)
        {
            inc_hour();
            dx--;
        }
        else
        {
            dec_hour();
            dx++;
        }
}

void datetime_t::shift_minute(int8_t dx)
{
    while (dx != 0)
        if (dx > 0)
        {
            inc_minute();
            dx--;
        }
        else
        {
            dec_minute();
            dx++;
        }
}
