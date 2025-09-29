// Максимальное значение уровня освещенности
constexpr const uint8_t LIGHT_LEVEL_MAX = 100;
// Максимальное значение времени выбрежки
constexpr const uint8_t LIGHT_EXPOSURE_MAX = 9;

// Структура настроек освещенности
struct light_settings_t
{
    // Ручной уровень
    uint8_t level;
    // Плавность смены
    uint8_t smooth;
    // Выдержка
    uint8_t exposure;
    // Автоподстройка
    bool autoset;
    // Ночной режим
    bool nightmode;
    
    // Проверка полей
    bool check(void) const
    {
        return exposure <= LIGHT_EXPOSURE_MAX &&
               level <= LIGHT_LEVEL_MAX &&
               smooth <= HMI_TIME_COUNT &&
               ipc_bool_check(autoset) &&
               ipc_bool_check(nightmode);
    }
};

// Команда получения настроек освещенности
class light_command_settings_get_t : public ipc_command_get_t<light_settings_t>
{
public:
    // Конструктор по умолчанию
    light_command_settings_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_LIGHT_SETTINGS_GET)
    { }
};

// Команда установки настроек освещенности
class light_command_settings_set_t : public ipc_command_set_t<light_settings_t>
{
public:
    // Конструктор по умолчанию
    light_command_settings_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_LIGHT_SETTINGS_SET)
    { }
};

// Структура ответа команды получения состояния освещенности
struct light_command_state_get_response_t
{
    uint8_t level;
    
    // Проверка полей
    bool check(void) const
    {
        return level <= LIGHT_LEVEL_MAX;
    }
};

// Команда получения состояния освещенности
class light_command_state_get_t : public ipc_command_get_t<light_command_state_get_response_t>
{
public:
    // Конструктор по умолчанию
    light_command_state_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_LIGHT_STATE_GET)
    { }
};
