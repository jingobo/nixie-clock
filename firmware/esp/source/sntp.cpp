#include "sntp.h"

// Переворачивает байты местами
ROM static void memswp(void *data, size_t size)
{
    size_t s2x = size / 2;
    uint8_t *buf = (uint8_t *)data;
    for (size_t i = 0; i < s2x; i++)
    {
        auto tmp = buf[i];
        buf[i] = buf[size - i - 1];
        buf[size - i - 1] = tmp;
    }
}

// Переворачивает байты местами (на системах с BE заменить пустым макросом)
#define SNTP_MEMSWP(x)              memswp(&(x), sizeof(x))

// Количество секунд в часу
#define SNTP_SECS_PER_HOUR          (60L * 60)
// Количество секунд в сутках
#define SNTP_SECS_PER_DAY           (24L * SNTP_SECS_PER_HOUR)
// Количество дней в году с учетом високосных лет (365.25) валидно для [1901..2099]
#define SNTP_DAYS_PER_YEAR          ((365L * 256) + (256 / 4))

// Поцелуй смерти (сервер не должен использоваться)
#define SNTP_STRATUM_DEADKISS(x)    ((x) == 0)
// Первичная синхронизация
#define SNTP_STRATUM_PRIMARY(x)     ((x) == 1)
// Вторичная синхронизация
#define SNTP_STRATUM_SECONDARY(x)   ((x) > 1 && (x) < 16)
// Зарезервированно
#define SNTP_STRATUM_RESERVED(x)    ((x) > 15 && (x) < 256)


// Подготавливает поля после записи
ROM void sntp_time_t::ready(void)
{
    SNTP_MEMSWP(seconds);
    SNTP_MEMSWP(fraction);
}

// Конвертирование в календарное представление
ROM void sntp_time_t::datetime_get(datetime_t &dest) const
{
    // Количество секунд с полночи
    auto dayclock = seconds % SNTP_SECS_PER_DAY;
    // Количество суток
    auto dayno = seconds / SNTP_SECS_PER_DAY;
    // Если дата переполнена
    if (!(seconds & 0x80000000))
    {
        // Смещаем секунды на 06:28:16
        dayclock += 23296;
        // Смещаем сутки до 02/07/2036
        dayno += 49710;
        dayno += dayclock / SNTP_SECS_PER_DAY;
        dayclock %= SNTP_SECS_PER_DAY;
    }
    // Определяем часы, минуты, секунды
    dest.second = (uint8_t)(dayclock % 60);
    dest.minute = (uint8_t)((dayclock % SNTP_SECS_PER_HOUR) / 60);
    dest.hour = (uint8_t)(dayclock / SNTP_SECS_PER_HOUR);
    // 1900 год не високосный
    dayno += 1;
    // Подсчет количества лет
    dayno <<= 8;
    dest.year = DATETIME_YEAR_MIN + (uint16_t)(dayno / SNTP_DAYS_PER_YEAR);
    // Количество дней в текущем году
    auto day = (int16_t)((dayno % SNTP_DAYS_PER_YEAR) >> 8);
    // Определение високосного года
    auto leap = dest.leap();
    // Подсчет месяца и дня
    auto month = 0;
    while (day >= 0)
        day -= datetime_t::month_day_count(++month, leap);
    dest.month = (uint8_t)month;
    dest.day = (uint8_t)(day + datetime_t::month_day_count(month, leap) + 1);
}

// Подготавливает поля после записи
ROM void sntp_packet_t::ready(void)
{
    time.ready();
    SNTP_MEMSWP(delay);
    SNTP_MEMSWP(dispersion);
    SNTP_MEMSWP(reference_id);
}

// Проверка на валидность ответа
ROM bool sntp_packet_t::response_check(void) const
{
    return
        (mode == SNTP_MODE_SERVER) &&
        (version > 2) &&
        (status != SNTP_STATUS_ALARM) &&
        (SNTP_STRATUM_PRIMARY(stratum) || SNTP_STRATUM_PRIMARY(stratum) || SNTP_STRATUM_SECONDARY(stratum));
}

