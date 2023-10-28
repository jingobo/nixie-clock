#include "sntp.h"

// Переворачивает байты местами
static void sntp_memswp(void *data, size_t size)
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

// Получает признак первичной синхронизации
static bool sntp_stratum_is_primary(uint8_t x)
{
    return x == 1;
}

// Получает признак вторичной синхронизации
static bool sntp_stratum_is_secondary(uint8_t x)
{
    return x > 1 && x < 16;
}

// Подготавливает поля после записи
void sntp_time_t::ready(void)
{
    sntp_memswp(&seconds, sizeof(seconds));
    sntp_memswp(&fraction, sizeof(fraction));
}

// Конвертирование в календарное представление
bool sntp_time_t::datetime_get(datetime_t &dest) const
{
    uint64_t utc_seconds = seconds;
    // Если дата переполнена
    if ((seconds & 0x80000000) == 0)
        utc_seconds += 0x0100000000;
    
    return datetime_t::from_utc_seconds(utc_seconds, dest);
}

// Подготавливает поля после записи
bool sntp_packet_t::ready(void)
{
    // Подготовка
    time.ready();
    sntp_memswp(&delay, sizeof(delay));
    sntp_memswp(&dispersion, sizeof(dispersion));
    sntp_memswp(&reference_id, sizeof(reference_id));
    
    // Проверка
    return (mode == SNTP_MODE_SERVER) &&
           (version > 2) &&
           (status != SNTP_STATUS_ALARM) &&
           (sntp_stratum_is_primary(stratum) || sntp_stratum_is_secondary(stratum));
}
