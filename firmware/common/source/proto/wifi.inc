// Тип данных для имени точки доступа
typedef char wifi_ssid_t[33];
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

// Структура настроек WIFI
ALIGN_FIELD_8
struct wifi_settings_t
{
	// Интерфейсы
	struct intf_t
	{
	    // Включена ли точка доступа
	    ipc_bool_t use : 8;
	    // Канал
	    uint8_t channel;
	    // Имя
	    wifi_ssid_t ssid;
	    // Пароль
	    wifi_password_t password;

	    // Оператор равенства
	    bool operator == (const intf_t &other)
	    {
	        return use == other.use &&
	               channel == other.channel &&
	               strcmp(ssid, other.ssid) == 0 &&
	               strcmp(password, other.password) == 0;
	    }

	    // Оператор неравенства
	    bool operator != (const intf_t &other)
	    {
	        return !(*this == other);
	    }

	    // Провера полей
	    bool check(void) const
	    {
	        return use <= IPC_BOOL_TRUE &&
	               channel <= 13 &&
	               (!use || wifi_ssid_check(ssid)) &&
	               (!use || wifi_password_check(password));
	    }

	    // Отчистка полей
	    void clear(void)
	    {
	        channel = 0;
	        use = IPC_BOOL_FALSE;
	        ssid[0] = password[0] = '\0';
	    }
	} intf[WIFI_INTF_COUNT];

    // Провера полей
    bool check(void) const
    {
    	for (auto i = 0; i < WIFI_INTF_COUNT; i++)
    		if (!intf[i].check())
    			return false;

        return true;
    }
    
    // Отчистка полей
    void clear(void)
    {
    	for (auto i = 0; i < WIFI_INTF_COUNT; i++)
    		intf[i].clear();
    }
};
ALIGN_FIELD_DEF

// Команда чтения настроек
class wifi_command_settings_get_t : public ipc_command_get_t<wifi_settings_t>
{
public:
    // Конструктор по умолчанию
    wifi_command_settings_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_WIFI_SETTINGS_GET)
    { }
};

// Команда записи настроек
class wifi_command_settings_set_t : public ipc_command_set_t<wifi_settings_t>
{
public:
    // Конструктор по умолчанию
    wifi_command_settings_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_WIFI_SETTINGS_SET)
    { }
};

// Команда оповещения изменения настроек
class wifi_command_settings_changed_t : public ipc_command_t
{
public:
    // Конструктор по умолчанию
    wifi_command_settings_changed_t(void) : ipc_command_t(IPC_OPCODE_ESP_WIFI_SETTINGS_CHANGED)
    { }
};

ALIGN_FIELD_8
// Данные запроса на поиск сетей с опросом
struct wifi_command_search_pool_request_t
{
	enum
	{
		// Максимальное количество сетей
		MAX_INDEX = 20,
	};

    // Команда
    enum
    {
        // Опрос состояния
        COMMAND_POOL = 0,
        // Запуск поиска
        COMMAND_START,
    } command : 8;
    
    // Индекс сети
    uint8_t index;

    // Провера полей
    bool check(void) const
    {
        return command <= COMMAND_START &&
        		index <= MAX_INDEX;
    }
};
ALIGN_FIELD_DEF

ALIGN_FIELD_8
// Данные ответа на поиск сетей с опросом
struct wifi_command_search_pool_response_t
{
    // Статус
    enum
    {
        // Простой (или конец списка)
        STATUS_IDLE = 0,
        // Запущен (идет поиск)
        STATUS_RUNNING,
        // Информация о сети
        STATUS_RECORD,
    } status : 8;
    
    // Информация о сети
    struct
    {
        // Имя
        wifi_ssid_t ssid;
        // Уровень сигнала
        int8_t rssi;
        // Признак приватности
        ipc_bool_t priv : 8;
    } record;
    
    // Провера полей
    bool check(void) const
    {
        if (status > STATUS_RECORD)
            return false;

        if (status != STATUS_RECORD)
            return true;
        
        return wifi_ssid_check(record.ssid) && 
               record.rssi < 0 &&
               record.priv <= IPC_BOOL_TRUE;
    }
};
ALIGN_FIELD_DEF

// Команда поиска сетей с опросом состояния
class wifi_command_search_pool_t : public ipc_command_fixed_t<wifi_command_search_pool_request_t, wifi_command_search_pool_response_t>
{
public:
    // Конструктор по умолчанию
    wifi_command_search_pool_t(void) : ipc_command_fixed_t(IPC_OPCODE_ESP_WIFI_SEARCH_POOL)
    { }
};

// Перечисление состояния интерфейса
enum wifi_intf_state_t
{
	// Простой
	WIFI_INTF_STATE_IDLE = 0,
	// Установка
	WIFI_INTF_STATE_ESTABLISH,
	// Готово
	WIFI_INTF_STATE_READY,
	// Ошибка
	WIFI_INTF_STATE_ERROR
};

ALIGN_FIELD_8
// Структура информации о интерфейсах
struct wifi_info_t
{
    // Данные интерфейса сети
    struct intf_t
    {
        // Состояние
        wifi_intf_state_t state : 8;
        // IP
        wifi_ip_t ip;
        
        // Конструктор по умолчанию
        intf_t(void) : state(WIFI_INTF_STATE_IDLE)
        { }
    } intf[WIFI_INTF_COUNT];

    // Провера полей
    bool check(void) const
    {
    	for (auto i = 0; i < WIFI_INTF_COUNT; i++)
    		if (intf[i].state > WIFI_INTF_STATE_ERROR)
    			return false;

        return true;
    }
};
ALIGN_FIELD_DEF

// Команда чтения информации о сети
class wifi_command_info_get_t : public ipc_command_get_t<wifi_info_t>
{
public:
    // Конструктор по умолчанию
	wifi_command_info_get_t(void) : ipc_command_get_t(IPC_OPCODE_ESP_WIFI_INFO_GET)
    { }
};

ALIGN_FIELD_8
// Данные запроса репортировании о присвоении IP адреса
struct wifi_ip_report_request_t
{
    // IP
    wifi_ip_t ip;
	// Интерфейс
	wifi_intf_t intf : 8;
    
    // Провера полей
    bool check(void) const
    {
        return intf < WIFI_INTF_COUNT;
    }
};
ALIGN_FIELD_DEF

// Команда репортировании о присвоении IP адреса
class wifi_command_ip_report_t : public ipc_command_set_t<wifi_ip_report_request_t>
{
public:
    // Конструктор по умолчанию
    wifi_command_ip_report_t(void) : ipc_command_set_t(IPC_OPCODE_STM_WIFI_IP_REPORT)
    { }
};
