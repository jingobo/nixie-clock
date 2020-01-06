// Тип данных для имени точки доступа
typedef char wifi_ssid_t[21];
// Тип данных для пароля точки доступа
typedef char wifi_password_t[13];

// Проверка имени точки доступа
static inline bool wifi_ssid_check(const wifi_ssid_t ssid)
{
    return ipc_string_length(ssid, sizeof(wifi_ssid_t)) > 0;
}

// Проверка пароля точки доступа
static inline bool wifi_password_check(const wifi_password_t password)
{
    auto len = ipc_string_length(password, sizeof(wifi_password_t));
    return len == 0 || len > 7;
}

// Структура части настроек WIFI
ALIGN_FIELD_8
struct wifi_settings_part_t
{
    // Включена ли точка доступа
    ipc_bool_t use : 8;
    // Имя
    wifi_ssid_t ssid;
    // Пароль
    wifi_password_t password;

    // Оператор равенства
    bool operator == (const wifi_settings_part_t &other)
    {
        return use == other.use &&
               strcmp(ssid, other.ssid) == 0 &&
               strcmp(password, other.password) == 0;
    }

    // Оператор неравенства
    bool operator != (const wifi_settings_part_t &other)
    {
        return !(*this == other);
    }

    // Провера полей
    bool check(void) const
    {
        return use <= IPC_BOOL_TRUE &&
               (!use || wifi_ssid_check(ssid)) &&
               (!use || wifi_password_check(password));
    }
    
    // Отчистка полей
    void clear(void)
    {
        use = IPC_BOOL_FALSE;
        ssid[0] = password[0] = '\0';
    }
};
ALIGN_FIELD_DEF

// Структура настроек WIFI
ALIGN_FIELD_8
struct wifi_settings_t
{
    // Режим точки доступа
    wifi_settings_part_t softap;
    // Режим подключения к точке доступа
    wifi_settings_part_t station;

    // Провера полей
    bool check(void) const
    {
        return softap.check() && station.check();
    }
    
    // Отчистка полей
    void clear(void)
    {
        softap.clear();
        station.clear();
    }
};
ALIGN_FIELD_DEF

// Данные ответа на полечение настроек WiFi
struct wifi_command_settings_get_response_t
{
    // Настройки
    wifi_settings_t settings;
    
    // Провера полей
    bool check(void) const
    {
        return settings.check();
    }
};

// Команда запроса настроек WiFi
class wifi_command_settings_get_t : public ipc_command_get_t<wifi_command_settings_get_response_t>
{
public:
    // Конструктор по умолчанию
    wifi_command_settings_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_WIFI_SETTINGS_GET)
    { }
};

// Команда оповещения изменения настроек WiFi
class wifi_command_settings_changed_t : public ipc_command_t
{
public:
    // Конструктор по умолчанию
    wifi_command_settings_changed_t(void) : ipc_command_t(IPC_OPCODE_ESP_WIFI_SETTINGS_CHANGED)
    { }
};
