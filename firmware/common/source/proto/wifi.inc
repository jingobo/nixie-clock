ALIGN_FIELD_8

// Структура IPv4 адреса
union wifi_ip_t
{
    // Сырое значение
    uint32_t raw;
    // По октетам
    uint8_t octets[4];

    // Конструктор по умолчанию
    wifi_ip_t(void) : raw(0)
    { }

    // Конструктор с указанием сырого значения
    wifi_ip_t(uint32_t ip) : raw(ip)
    { }

    // Конструктор с указанием октетов
    wifi_ip_t(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4)
    {
        octets[0] = o1;
        octets[1] = o2;
        octets[2] = o3;
        octets[3] = o4;
    }
};

// Тип данных для имени точки доступа
typedef char wifi_ssid_t[21];
// Тип данных для пароля точки доступа
typedef char wifi_password_t[13];

// Проверка имени точки доступа
ROM static bool wifi_ssid_check(const wifi_ssid_t ssid)
{
    return ipc_string_length(ssid, sizeof(wifi_ssid_t)) > 0;
}

// Проверка пароля точки доступа
ROM static bool wifi_password_check(const wifi_password_t password)
{
    auto len = ipc_string_length(password, sizeof(wifi_password_t));
    return len == 0 || len > 7;
}

// Структура настроек WIFI
struct wifi_settings_t
{
    // Режим точки доступа
    struct
    {
        // Включена ли точка доступа
        ipc_bool_t use : 8;
        // Номер канала [1..13]
        uint8_t channel;
        // Имя
        wifi_ssid_t ssid;
        // Пароль
        wifi_password_t password;
    } softap;
    // Режим подключения к точке доступа
    struct
    {
        // Включена ли точка доступа
        ipc_bool_t use : 8;
        // Имя
        wifi_ssid_t ssid;
        // Пароль
        wifi_password_t password;
    } station;
};

// Данные запроса команды установки настроек WIFI
struct wifi_command_settings_request_t
{
    // Настройки
    wifi_settings_t settings;
    
    // Провера полей
    ROM bool check(void) const
    {
        return
            settings.softap.use <= IPC_BOOL_TRUE &&
            settings.station.use <= IPC_BOOL_TRUE &&
            settings.softap.channel > 0 && settings.softap.channel < 14 &&
            (!settings.softap.use || wifi_ssid_check(settings.softap.ssid)) &&
            (!settings.station.use || wifi_ssid_check(settings.station.ssid)) &&
            (!settings.softap.use || wifi_password_check(settings.softap.password)) &&
            (!settings.station.use || wifi_password_check(settings.station.password));
    }
};

// Команда установки настроек WIFI
class wifi_command_settings_t : public ipc_command_data_template_notify_t<wifi_command_settings_request_t>
{
public:
    // Конструктор по умолчанию
    wifi_command_settings_t(void) : ipc_command_data_template_notify_t(IPC_COMMAND_STM_WIFI_SETTINGS)
    { }
};

// Команда запроса настроек WIFI
class wifi_command_settings_require_t : public ipc_command_data_empty_t
{
public:
    // Конструктор по умолчанию
    wifi_command_settings_require_t(void) : ipc_command_data_empty_t(IPC_COMMAND_ESP_WIFI_SETTINGS_REQUIRE)
    { }
};

ALIGN_FIELD_DEF
