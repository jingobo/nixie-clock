// Структура настроек дисплея
struct display_settings_t
{
    // Подсветка
    led_source_t::settings_t led;
    // Неонки
    neon_source_t::settings_t neon;
    // Лампы
    nixie_switcher_t::settings_t nixie;

    // Проверка полей
    bool check(void) const
    {
        return 
            // Лампы
           nixie.effect <= nixie_switcher_t::EFFECT_SWITCH_OUT &&
           // Светодиоды
           led.source <= led_source_t::DATA_SOURCE_ANY_RANDOM &&
           led.effect <= led_source_t::EFFECT_OUT &&
           led.smooth <= HMI_TIME_COUNT &&
           // Неонки
           neon.mask <= neon_source_t::RANK_MASK_ALL &&
           neon.smooth <= HMI_TIME_COUNT &&
           ipc_bool_check(neon.inversion) &&
           neon.period <= HMI_TIME_COUNT;
    }
};

STATIC_ASSERT(sizeof(display_settings_t) == 25);

// Структура настроек дисплея с опцией показа
struct display_settings_arm_t
{
    // Базовые настройки
    display_settings_t base;
    
    // Показывать
    bool allow;

    // Проверка полей
    bool check(void) const
    {
        return base.check() && 
               ipc_bool_check(allow);
    }
};

STATIC_ASSERT(sizeof(display_settings_arm_t) == 26);

// Структура настроек дисплея с опцией таймаута
struct display_settings_timeout_t
{
    // Базовые настройки
    display_settings_arm_t base;
    
    // Таймаут в секундах
    uint8_t timeout;

    // Проверка полей
    bool check(void) const
    {
        return timeout <= 30 && 
               base.check();
    }
};

STATIC_ASSERT(sizeof(display_settings_timeout_t) == 27);

// Команда запрос настроек сцены времени
class display_command_time_get_t : public ipc_command_get_t<display_settings_t>
{
public:
    // Конструктор по умолчанию
    display_command_time_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_DISPLAY_TIME_GET)
    { }
};

// Команда установки настроек сцены времени
class display_command_time_set_t : public ipc_command_set_t<display_settings_t>
{
public:
    // Конструктор по умолчанию
    display_command_time_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_DISPLAY_TIME_SET)
    { }
};

// Команда запрос настроек сцены даты
class display_command_date_get_t : public ipc_command_get_t<display_settings_timeout_t>
{
public:
    // Конструктор по умолчанию
    display_command_date_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_DISPLAY_DATE_GET)
    { }
};

// Команда установки настроек сцены даты
class display_command_date_set_t : public ipc_command_set_t<display_settings_timeout_t>
{
public:
    // Конструктор по умолчанию
    display_command_date_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_DISPLAY_DATE_SET)
    { }
};
