#include "wifi.h"
#include "core.h"
#include "task.h"
#include "system.h"

#include <proto/wifi.inc>

// Имя модуля для логирования
#define MODULE_NAME     "WIFI"

// Класс обслуживания WIFI
static class wifi_t : ipc_handler_event_t, public task_wrapper_t
{
    // Последние полученные настроейки
    wifi_settings_t settings;

    // Обработчик команды получения настроек WIFI
    class handler_command_settings_t : public ipc_handler_command_template_t<wifi_command_settings_t>
    {
        // Родительский объект
        wifi_t &wifi;
        // Состояние
        enum
        {
            // Простой
            STATE_IDLE = 0,
            // Запрос настроек
            STATE_REQUEST,
            // Ожидание настроек
            STATE_RESPONSE
        } state;
    protected:
        // Оповещение о поступлении данных
        virtual void notify(ipc_dir_t dir);
    public:
        // Конструктор по умолчанию
        handler_command_settings_t(wifi_t &parent) : wifi(parent), state(STATE_REQUEST)
        { }

        // Обработчик простоя
        ROM void idle(void)
        {
            if (state != STATE_REQUEST)
                return;
            wifi_command_settings_require_t command;
            state = core_transmit(IPC_DIR_REQUEST, command) ? STATE_RESPONSE : STATE_REQUEST;
        }

        // Обработчик сброса
        RAM void reset(void)
        {
            if (state == STATE_RESPONSE)
                state = STATE_REQUEST;
        }
    } handler_command_settings;
protected:
    // Событие простоя
    virtual void idle(void);
    // Событие сброса
    virtual void reset(void);
    // Обработка задачи
    virtual void execute(void);
public:
    // Конструктор по умолчанию
    wifi_t(void) : handler_command_settings(*this), task_wrapper_t("wifi")
    { }

    // Инициализация
    ROM void init(void)
    {
        // Обработчики
        core_handler_add_event(*this);
        core_handler_add_command(handler_command_settings);
        // Светодиод
        wifi_status_led_install(5, PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
    }
} wifi;

ROM void wifi_t::idle(void)
{
    // Базовый метод
    ipc_handler_event_t::idle();
    // Если нужно получить настройки
    handler_command_settings.idle();
}

ROM void wifi_t::reset(void)
{
    // Сброс окманд
    handler_command_settings.reset();
    // Базовый метод
    ipc_handler_event_t::reset();
}

ROM void wifi_t::execute(void)
{
    // Получаем настройки
    MUTEX_ENTER(wifi.mutex);
        auto local = settings;
        __sync_synchronize();
    MUTEX_LEAVE(wifi.mutex);
    // Отключение WIFI
    auto mode = wifi_get_opmode();
    // Если включен режим подключения к точке
    if (mode & STATION_MODE)
    {
        // Стоп DHPC
        if (wifi_station_dhcpc_status() && !wifi_station_dhcpc_stop())
            log_module(MODULE_NAME, "Unable to suspend DHPC!");
        // Разъединение
        if (wifi_station_get_connect_status() && !wifi_station_disconnect())
            log_module(MODULE_NAME, "Unable to disconnect from AP!");
    }
    // Если включен режим точки доступа
    if (mode & SOFTAP_MODE)
    {
        // Стоп DHPC
        if (wifi_softap_dhcps_status() && !wifi_softap_dhcps_stop())
            log_module(MODULE_NAME, "Unable to suspend DHPC!");
    }
    // Отключение WIFI и установка режима
    mode = NULL_MODE;
    if (!wifi_set_opmode(mode))
        log_module(MODULE_NAME, "Error disable WIFI");
    if (wifi_get_phy_mode() != PHY_MODE_11B)
        log_module(MODULE_NAME, "Setup 802.11b mode %s", wifi_set_phy_mode(PHY_MODE_11B) ? "done." : "fail!");
    // Задание конфигурации подключения к точке
    if (local.station.use)
    {
        station_config cfg;
        // Заполнение настроек
        memset(&cfg, 0, sizeof(cfg));
        strcpy((char *)cfg.ssid, local.station.ssid);
        strcpy((char *)cfg.password, local.station.password);
        // Применение
        mode = (WIFI_MODE)(mode | STATION_MODE);
        if (!wifi_set_opmode(mode))
            log_module(MODULE_NAME, "Unable to init station!");
        else if (!wifi_station_set_config_current(&cfg))
            log_module(MODULE_NAME, "Unable to apply station settings!");
        else if (!wifi_station_connect())
            log_module(MODULE_NAME, "Unable to start station!");
        else
            log_module(MODULE_NAME, "Connecting to %s...", local.station.ssid);
        if (!wifi_station_dhcpc_start())
            log_module(MODULE_NAME, "Unable to start station DHPC!");
    }
    // Задание конфигурации точки доступа
    if (local.softap.use)
    {
        softap_config cfg;
        // Заполнение настроек
        memset(&cfg, 0, sizeof(cfg));
        strcpy((char *)cfg.ssid, local.softap.ssid);
        strcpy((char *)cfg.password, local.softap.password);
        cfg.channel = local.softap.channel;
        cfg.authmode = (strlen(local.softap.password) > 0) ? AUTH_WPA2_PSK : AUTH_OPEN;
        cfg.max_connection = 4;
        cfg.beacon_interval = 100;
        // Применение
        mode = (WIFI_MODE)(mode | SOFTAP_MODE);
        if (!wifi_set_opmode(mode))
            log_module(MODULE_NAME, "Unable to init softap!");
        else if (!wifi_softap_set_config_current(&cfg))
            log_module(MODULE_NAME, "Unable to apply softap settings!");
        else
            log_module(MODULE_NAME, "Created to %s softap.", local.softap.ssid);
        if (!wifi_softap_dhcps_start())
            log_module(MODULE_NAME, "Unable to start softap DHPC!");
    }
}

ROM void wifi_t::handler_command_settings_t::notify(ipc_dir_t dir)
{
    // Можем обрабатывать только запрос
    assert(dir == IPC_DIR_REQUEST);
    // Получены настройки
    state = STATE_IDLE;
    MUTEX_ENTER(wifi.mutex);
        wifi.settings = data.request.settings;
    MUTEX_LEAVE(wifi.mutex);
    log_module(MODULE_NAME, "Settings fetched");
    // Запуск задачи на применение настроек
    wifi.start(true);
}

RAM void wifi_init(void)
{
    wifi.init();
}

ROM void wifi_init_entry(void)
{
    // Запрет автоподключения в точке доступа
    if (wifi_station_get_auto_connect())
        wifi_station_set_auto_connect(false);
    // Разрешение переподключения после отключения от точки доступа
    if (!wifi_station_get_reconnect_policy())
        wifi_station_set_reconnect_policy(true);
    // Отключение WiFi
    wifi_set_opmode(NULL_MODE);
}

