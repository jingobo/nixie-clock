#ifndef __SNTP_H
#define __SNTP_H

#include "datetime.h"

// Режим работы
enum sntp_mode_t
{
    // Зарезервирован
    SNTP_MODE_RESERVED = 0,
    // Симметричный активный
    SNTP_MODE_ACTIVE,
    // Симметричный пассивный
    SNTP_MODE_PASSIVE,
    // Клиент
    SNTP_MODE_CLIENT,
    // Сервер
    SNTP_MODE_SERVER,
    // Широковещательный
    SNTP_MODE_BROADCAST,
    // Зарезервирован для управляющих сообщений NTP
    SNTP_MODE_RESERVED_CONTROL,
    // Зарезервирован для частного использования
    SNTP_MODE_RESERVED_PRIVATE,
};

// Статус коррекции
enum sntp_status_t
{
    // Нет коррекции
    SNTP_STATUS_NONE = 0,
    // Последняя минута будет иметь 61 секунду
    SNTP_STATUS_PLUS,
    // Последняя минута будет иметь 59 секунд
    SNTP_STATUS_MINUS,
    // Время не синхронизировано
    SNTP_STATUS_ALARM,
};

ALIGN_FIELD_8
// Время в формате SNTP
struct sntp_time_t
{
    // Целая часть секунд
    uint32_t seconds;
    // Дробная часть секунд
    uint32_t fraction;

    // Подготавливает поля после записи
    void ready(void);
    // Конвертирование в календарное представление
    bool datetime_get(datetime_t &dest) const;
};

// Сырой пакет SNTP
struct sntp_packet_t
{
    union
    {
        // Сырое значение заголовка
        uint8_t header;
        // Поля заголовка
        struct
        {
            // Режим работы
            sntp_mode_t mode : 3;
            // Номер версии
            uint8_t version : 3;
            // Индикатор коррекции
            sntp_status_t status : 2;
        };
    };
    // Страта
    uint8_t stratum;
    // Интервал опроса
    uint8_t poll;
    // Точность
    uint8_t precision;

    // Задержка
    uint32_t delay;
    // Дисперсия
    uint32_t dispersion;
    // Идентификатор источника
    uint32_t reference_id;
    // Времена
    struct
    {
        // Время обновления
        sntp_time_t update;
        // Начальное время
        sntp_time_t initial;
        // Время приёма
        sntp_time_t rx;
        // Время отправки
        sntp_time_t tx;

        // Подготавилвает поля после записи
        void ready(void)
        {
            update.ready();
            initial.ready();
            rx.ready();
            tx.ready();
        }
    } time;

    // Конструктор по умолчанию
    sntp_packet_t()
    {
    	MEMORY_CLEAR_PTR(this);
        mode = SNTP_MODE_CLIENT;
        version = 3;
    }

    // Подготавливает поля после записи
    bool ready(void);
};
ALIGN_FIELD_DEF

#endif // __SNTP_H
