// Данные ответа запроса даты/времени
ALIGN_FIELD_8
struct datetime_command_get_response_t
{
    // Дата/время (возможно не валидная)
    datetime_t datetime;
    // Состояние
    enum
    {
        // Дата/время валидны
        STATUS_VALID = 0,
        // Ошибка сети
        STATUS_NETWORK,
        // Нет списка хостов
        STATUS_HOSTLIST,
    } status : 8;
    
    // Провера полей
    bool check(void) const
    {
        if (status > STATUS_HOSTLIST)
            return false;
        return status > STATUS_VALID || datetime.check();
    }
};
ALIGN_FIELD_DEF

// Команда запроса даты/времени
class datetime_command_get_t : public ipc_command_getter_t<datetime_command_get_response_t>
{
public:
    // Конструктор по умолчанию
    datetime_command_get_t(void) : ipc_command_getter_t(IPC_OPCODE_ESP_DATETIME_GET)
    { }
};

// Список серверов, построчно
typedef char datetime_hosts_data_t[251];

/* Проверка списка (поверхностная, что бы не давало сбой)
 * Строка должна состоять из символов ['a'..'z', 'A'..'Z', '0'..'9', '.', '-'],
 * содержать минимум 1 симвыол. Строки разделены символом '\n', заканчиваться на
 * символ '\0'. */
static inline bool datetime_hosts_data_check(const datetime_hosts_data_t hosts)
{
    // Проверка аргументов
    assert(hosts != NULL);
    // Обход символов
    auto len = 0;
    for (auto i = 0; i < sizeof(datetime_hosts_data_t); i++)
    {
        auto c = hosts[i];
        switch (c)
        {
            // Конец списка
            case '\0':
                return i == 0 || len > 0;
            // Конец строки
            case '\n':
                // Длинна минимум 1 символ
                if (len < 1)
                    return false;
                len = 0;
                break;
            default:
                if ((c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') ||
                    c == '.' || c == '-')
                {
                    len++;
                    break;
                }
                return false;
        }
    }
    return false;
}

// Копирование списка хостов
static inline void datetime_hosts_data_copy(datetime_hosts_data_t dest, const datetime_hosts_data_t source)
{
    // Проверка аргументов
    assert(dest != NULL && source != NULL);
    // Копирование
    memcpy(dest, source, sizeof(datetime_hosts_data_t));
}

// Данные запроса команды установки хостов SNTP
ALIGN_FIELD_8
struct datetime_command_hosts_set_request_t
{
    // Список хостов
    datetime_hosts_data_t hosts;
    
    // Провера полей
    bool check(void) const
    {
        return datetime_hosts_data_check(hosts);
    }
};
ALIGN_FIELD_DEF

// Команда отправки списка серверов
class datetime_command_hosts_set_t : public ipc_command_setter_t<datetime_command_hosts_set_request_t>
{
public:
    // Конструктор по умолчанию
    datetime_command_hosts_set_t(void) : ipc_command_setter_t(IPC_OPCODE_ESP_DATETIME_HOSTS_SET)
    { }
};

ALIGN_FIELD_8
// Структура настроек даты/времени
struct datetime_sync_settings_t
{
    // Период синхронизации
    enum sync_period_t
    {
        // Раз в час
        SYNC_PERIOD_HOUR,
        // Раз в день
        SYNC_PERIOD_DAY,
        // Раз в неделю
        SYNC_PERIOD_WEEK,
        // Раз в месяц
        SYNC_PERIOD_MONTH
    };

    // Включена ли синхронизация
    ipc_bool_t sync : 8;
    // Часовой пояс
    int8_t timezone;
    // Смещение часа
    int8_t offset;
    // Период
    sync_period_t period : 8;
    // Список серверов
    datetime_hosts_data_t hosts;
};
ALIGN_FIELD_DEF
