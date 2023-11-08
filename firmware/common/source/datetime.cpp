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
    constexpr const uint8_t MONTH_DAYS[MONTH_MAX] = { 31,  0,  31,  30,  31,  30,  31,  31,  30,  31,  30,  31 };
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

// Смещение года UTC
constexpr const uint16_t DATETIME_UTC_YEAR_BASE = 1900;
// Фиксированная точка для расчета дней
constexpr const uint32_t DATETIME_DAYS_FP = 256;
// Количество дней в году с учетом високосных лет (365.25) валидно для [1901..2099]
constexpr const uint32_t DATETIME_DAYS_PER_YEAR = 365 * DATETIME_DAYS_FP + DATETIME_DAYS_FP / 4;

bool datetime_t::utc_from_seconds(uint64_t seconds, datetime_t &dest)
{
    // Расчет часов, минут, секунд
    {
        // Количество секунд в сутках
        const auto day_seconds = (uint32_t)(seconds % SECONDS_PER_DAY);
        
        dest.second = (uint8_t)(day_seconds % SECONDS_PER_MINUTE);
        dest.minute = (uint8_t)(day_seconds % SECONDS_PER_HOUR / SECONDS_PER_MINUTE);
        dest.hour = (uint8_t)(day_seconds / SECONDS_PER_HOUR);
    }

    // Количество суток
    auto day_count = (uint32_t)(seconds / SECONDS_PER_DAY);
    // 1900 год не високосный
    day_count += 1;

    // Расчет года
    day_count *= DATETIME_DAYS_FP;
    const auto year = DATETIME_UTC_YEAR_BASE + (uint16_t)(day_count / DATETIME_DAYS_PER_YEAR);
    if (year < YEAR_BASE)
        // Год не корректный
        return false;
    dest.year = (uint8_t)(year - YEAR_BASE);
    
    // Расчет месяца и дня
    {
        // Количество дней в текущем году
        auto day_year = (uint16_t)(day_count % DATETIME_DAYS_PER_YEAR / DATETIME_DAYS_FP);
        
        // Цикл по месяцам
        for (dest.month = MONTH_MIN;; dest.month++)
        {
            // Количество дней в месяце
            const auto mday_count = dest.month_day_count();
            
            // Вычет дней месяца из дней года
            if (day_year < mday_count)
                break;
            day_year -= mday_count;
        }
        
        dest.day = (uint8_t)day_year + DAY_MIN;
    }
    
    // Резеультат
    return dest.check();    
}

uint32_t datetime_t::utc_day_count(void) const
{
    assert(check());

    // Общее количество дней
    uint32_t result;
    
    // Учет месяца и дня
    {
        // Признак високосного года
        const auto is_leap = leap();
        
        // Цикл по месяцам
        uint16_t day_year = day - DAY_MIN;
        for (auto i = MONTH_MIN; i < month; i++)
            day_year += month_day_count(i, is_leap);
            
        // Расчет дня текущего года
        result = day_year * DATETIME_DAYS_FP;
    }
    
    // Учет года
    {
        // Годов в днях
        const auto year_days = (year + YEAR_BASE - DATETIME_UTC_YEAR_BASE) * DATETIME_DAYS_PER_YEAR;
        result += year_days;

        // Округление в большую сторону
        if ((year_days % DATETIME_DAYS_FP) != 0)
            result += DATETIME_DAYS_FP;
    }
    result /= DATETIME_DAYS_FP;

    // 1900 год не високосный
    return result - 1;
}

uint64_t datetime_t::utc_to_seconds(void) const
{
    // Перевод дней в секунды
    uint64_t result = utc_day_count();
    result *= SECONDS_PER_DAY;
    
    // Учет часов, минут, секунд
    result += second;
    result += minute * SECONDS_PER_MINUTE;
    result += hour * SECONDS_PER_HOUR;
    
    return result;
}

uint8_t datetime_t::day_week(void) const
{
    return (uint8_t)(utc_day_count() % WDAY_COUNT);
}
