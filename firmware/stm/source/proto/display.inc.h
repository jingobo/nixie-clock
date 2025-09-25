// Структура настроек дисплея
struct display_settings_t
{
    // Подсветка
    led_source_t::settings_t led;
    // Неонки
    neon_source_t::settings_t neon;
    // Эффект ламп
    nixie_switcher_t::effect_t nixie;

    // Проверка полей
    bool check(void) const
    {
        return 
           // Светодиоды
           led.source <= led_source_t::DATA_SOURCE_ANY_RANDOM &&
           led.effect <= led_source_t::EFFECT_OUT &&
           led.smooth <= HMI_TIME_COUNT &&
            // Лампы
           nixie <= nixie_switcher_t::EFFECT_SWITCH_OUT &&
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

// Структура настроек сцены времени
struct display_settings_time_t
{
    // Базовые настройки
    display_settings_t base;
    // Эффект ламп для секунд
    nixie_switcher_t::effect_t nixie_second;

    // Проверка полей
    bool check(void) const
    {
        return base.check() && 
               nixie_second <= nixie_switcher_t::EFFECT_SWITCH_OUT;
    }
};

STATIC_ASSERT(sizeof(display_settings_time_t) == 26);

// Команда запрос настроек сцены времени
class display_command_time_get_t : public ipc_command_get_t<display_settings_time_t>
{
public:
    // Конструктор по умолчанию
    display_command_time_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_DISPLAY_TIME_GET)
    { }
};

// Команда установки настроек сцены времени
class display_command_time_set_t : public ipc_command_set_t<display_settings_time_t>
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

// Команда запрос настроек сцены своей сети
class display_command_onet_get_t : public ipc_command_get_t<display_settings_timeout_t>
{
public:
    // Конструктор по умолчанию
    display_command_onet_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_ONET_SETTINGS_GET)
    { }
};

// Команда установки настроек сцены своей сети
class display_command_onet_set_t : public ipc_command_set_t<display_settings_timeout_t>
{
public:
    // Конструктор по умолчанию
    display_command_onet_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_ONET_SETTINGS_SET)
    { }
};

// Команда запрос настроек сцены подключенной сети
class display_command_cnet_get_t : public ipc_command_get_t<display_settings_timeout_t>
{
public:
    // Конструктор по умолчанию
    display_command_cnet_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_CNET_SETTINGS_GET)
    { }
};

// Команда установки настроек сцены подключенной сети
class display_command_cnet_set_t : public ipc_command_set_t<display_settings_timeout_t>
{
public:
    // Конструктор по умолчанию
    display_command_cnet_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_CNET_SETTINGS_SET)
    { }
};
