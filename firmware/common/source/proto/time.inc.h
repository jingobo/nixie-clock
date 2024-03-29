﻿// Данные ответа запроса синхронизации даты/времени
struct time_command_sync_response_t
{
    // Дата/время (возможно не валидная)
    datetime_t value;
    // Состояние
    enum : uint8_t
    {
        // Дата/время валидны
        STATUS_SUCCESS = 0,
        // Дата/время не получены
        STATUS_FAILED,
        // Нет списка хостов
        STATUS_HOSTLIST,
    } status;
    
    // Провера полей
    bool check(void) const
    {
        if (status > STATUS_HOSTLIST)
            return false;
        return status > STATUS_SUCCESS || value.check();
    }
};

// Команда синхронизации даты/времени
class time_command_sync_t : public ipc_command_get_t<time_command_sync_response_t>
{
public:
    // Конструктор по умолчанию
    time_command_sync_t(void) : ipc_command_get_t(IPC_OPCODE_ESP_TIME_SYNC)
    { }
};

// Максимум символов (с терминальным) на один хост
#define TIME_HOSTANME_CHARS_MAX     65
// Минимум символов (без терминального) на один хост
#define TIME_HOSTANME_CHARS_MIN     3

// Список серверов, построчно
typedef char time_hosts_data_t[TIME_HOSTANME_CHARS_MAX * 4];

/* Проверка списка (поверхностная, что бы не давало сбой)
 * Строка должна состоять из символов ['a'..'z', 'A'..'Z', '0'..'9', '.', '-'],
 * содержать минимум 3 симвыоли и не более 64. Строки разделены символом '\n',
 * заканчиваться на символ '\0'. */
static bool time_hosts_data_check(const time_hosts_data_t hosts)
{
    // Проверка аргументов
    assert(hosts != NULL);
    // Обход символов
    auto len = 0;
    for (auto i = 0; i < sizeof(time_hosts_data_t); i++)
    {
        auto c = hosts[i];
        switch (c)
        {
            // Конец списка
            case '\0':
                return i == 0 || len >= TIME_HOSTANME_CHARS_MIN;
            // Конец строки
            case '\n':
                // Длинна минимум 3 символа
                if (len < TIME_HOSTANME_CHARS_MIN)
                    return false;
                len = 0;
                break;
            default:
                if ((c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') ||
                    c == '.' || c == '-')
                {
                    if (++len > TIME_HOSTANME_CHARS_MAX - 1)
                        return false;
                    break;
                }
                return false;
        }
    }
    return false;
}

// Копирование списка хостов
static void time_hosts_data_copy(time_hosts_data_t dest, const time_hosts_data_t source)
{
    // Проверка аргументов
    assert(dest != NULL && source != NULL);
    // Копирование
    memcpy(dest, source, sizeof(time_hosts_data_t));
}

// Проверяет, пуст ли список хостов
static bool time_hosts_empty(const time_hosts_data_t source)
{
    return source[0] == '\0';
}

// Данные запроса команды записи хостов
struct time_command_hostlist_set_request_t
{
    // Список хостов
    time_hosts_data_t hosts;
    
    // Провера полей
    bool check(void) const
    {
        return time_hosts_data_check(hosts);
    }
};

// Команда записи списка серверов
class time_command_hostlist_set_t : public ipc_command_set_t<time_command_hostlist_set_request_t>
{
public:
    // Конструктор по умолчанию
    time_command_hostlist_set_t(void) : ipc_command_set_t(IPC_OPCODE_ESP_TIME_HOSTLIST_SET)
    { }
};

// Структура ответа команды чтения текущей даты/времени
struct time_command_current_get_response_t
{
    ALIGN_FIELD_8
    struct
    {
        // Текущее время
        datetime_t current;
        // Время синхронизации
        datetime_t sync;
        // Секунд с запуска
        uint32_t uptime;
        // Частота часового кварца
        uint16_t lse;
    } time;
    ALIGN_FIELD_DEF
    
    // Можно ли запустить синхронизацию
    bool sync_allow;

    // Проверка полей
    bool check(void) const
    {
        return ipc_bool_check(sync_allow) &&
               time.current.check() &&
               (time.sync.check() || time.sync.empty());
    }
};

// Команда чтения текущей даты/времени
class time_command_current_get_t : public ipc_command_get_t<time_command_current_get_response_t>
{
public:
    // Конструктор по умолчанию
    time_command_current_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_TIME_GET)
    { }
};

// Команда записи текущей даты/времени
class time_command_current_set_t : public ipc_command_set_t<datetime_t>
{
public:
    // Конструктор по умолчанию
    time_command_current_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_TIME_SET)
    { }
};

// Структура настроек даты/времени
struct time_sync_settings_t
{
    // Включена ли синхронизация
    bool sync;
    // Часовой пояс
    int8_t timezone;
    // Смещение часа
    int8_t offset;
    // Список серверов
    time_hosts_data_t hosts;
    
    // Получает, можно ли производить синхронизацию
    bool sync_allow(void) const
    {
        return sync && !time_hosts_empty(hosts);
    }
    
    // Проверка полей
    bool check(void) const
    {
        return ipc_bool_check(sync) &&
               timezone >= -24 && timezone <= 26 &&
               offset >= -24 && offset <= 24 &&
               time_hosts_data_check(hosts);
    }
};

// Команда чтения настроек даты/времени
class time_command_settings_get_t : public ipc_command_get_t<time_sync_settings_t>
{
public:
    // Конструктор по умолчанию
    time_command_settings_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_TIME_SETTINGS_GET)
    { }
};

// Команда записи настроек даты/времени
class time_command_settings_set_t : public ipc_command_set_t<time_sync_settings_t>
{
public:
    // Конструктор по умолчанию
    time_command_settings_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_TIME_SETTINGS_SET)
    { }
};

// Данные запроса команды запуска синхронизации даты/времени
struct time_command_sync_start_request_t
{
    // Действие
    enum : uint8_t
    {
        // Запуск
        ACTION_START = 0,
        // Проверка результата
        ACTION_CHECK,
    } action;
    
    // Провера полей
    bool check(void) const
    {
        return action <= ACTION_CHECK;
    }
};

// Данные ответа команды запуска синхронизации даты/времени
struct time_command_sync_start_response_t
{
    // Статус обработки
    enum : uint8_t
    {
        // Синхронизация выполнена
        STATUS_SUCCESS = 0,
        // Синхронизация не может быть выполнена
        STATUS_FAILED,
        // Синхронизация в обработке
        STATUS_PENDING,
    } status;
    
    // Провера полей
    bool check(void) const
    {
        return status <= STATUS_PENDING;
    }
};

// Команда запуска синхронизации даты/времени
class time_command_sync_start_t : public ipc_command_fixed_t<time_command_sync_start_request_t, time_command_sync_start_response_t>
{
public:
    // Конструктор по умолчанию
    time_command_sync_start_t(void) : ipc_command_fixed_t(IPC_OPCODE_STM_TIME_SYNC_START)
    { }
};
