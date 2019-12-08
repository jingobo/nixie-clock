#include <stm.h>
#include <log.h>
#include <core.h>
#include <tool.h>
#include "wifi.h"
#include <system.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <tcpip_adapter.h>

#include <proto/wifi.inc>

// Имя модуля для логирования
LOG_TAG_DECL("WIFI");

// Обработчик команды запроса настроек WIFI
class wifi_command_handler_settings_get_t : public ipc_command_handler_template_t<wifi_command_settings_get_t>
{
    // Состояние
    enum
    {
        // Простой
        STATE_IDLE = 0,
        // Запрос настроек
        STATE_REQUEST,
        // Ожидание настроек
        STATE_RESPONSE
    } state = STATE_REQUEST;
    // Количество тиков с момента запроса
    os_tick_t request_time;
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir);
public:
    // Обработчик события сброса
    void reset(void)
    {
        // Сброс в начальное состояние
        if (state == STATE_RESPONSE)
            state = STATE_REQUEST;
    }

    // Обработчик события простоя
    void idle(void)
    {
        // Определяем что делать
        switch (state)
        {
            case STATE_REQUEST:
                // Запрос
                break;
            case STATE_RESPONSE:
                // Если прошло много времени с момента запроса
                if (os_tick_get() - request_time < OS_MS_TO_TICKS(5000))
                    return;
                break;
            default:
                return;
        }
        // Запрос
        if (!command.transmit(core_processor_out.esp, IPC_DIR_REQUEST))
            return;
        LOGI("Settings requested...");
        request_time = os_tick_get();
        state = STATE_RESPONSE;
    }
} wifi_command_handler_settings_get;

// Обработчик команды оповещения о смене настроек WiFi
static class wifi_command_handler_settings_changed_t : public ipc_command_handler_template_sticky_t<wifi_command_settings_changed_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать запрос
        assert(dir == IPC_DIR_REQUEST);
        // Перезапрашиваем настройки
        wifi_command_handler_settings_get.reset();
        // Отправляем ответ
        transmit();
        // Лог
        LOGW("Settings changed!");
    }

    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        return command.transmit(core_processor_out.esp, IPC_DIR_RESPONSE);
    }

} wifi_command_handler_settings_changed;

// Базовый класс обработчика событий
class wifi_ipc_events_t : public ipc_event_handler_t
{
protected:
    // Событие простоя
    virtual void idle(void)
    {
        // Базовый метод
        ipc_event_handler_t::idle();
        // Команда
        wifi_command_handler_settings_get.idle();
        wifi_command_handler_settings_changed.idle();
    }

    // Событие сброса
    virtual void reset(void)
    {
        // Базовый метод
        ipc_event_handler_t::reset();
        // Команда
        wifi_command_handler_settings_get.reset();
        wifi_command_handler_settings_changed.reset();
    }
} wifi_ipc_events;

// Вывод в лог MAC адрес указанного интерфейса
static void wifi_log_mac(const char name[], wifi_interface_t intf)
{
    uint8_t mac_raw[6];
    tool_mac_buffer_t mac_str;
    // Получаем адрес
    ESP_ERROR_CHECK(esp_wifi_get_mac(intf, mac_raw));
    // Конвертирование в строку и вывод
    tool_mac_to_string(mac_raw, mac_str);
    LOGI("%s MAC %s", name, mac_str);
}

// Вывод в лог информации о региональных настройках
static void wifi_log_country_info(void)
{
    wifi_country_t info_country;
    // Получаем информацию о региональных настройках
    ESP_ERROR_CHECK(esp_wifi_get_country(&info_country));
    // Вывод
    LOGI("Country: %s", info_country.cc);
    LOGI("Max TX power: %d", info_country.max_tx_power);
    LOGI("Start channel: %d", info_country.schan);
    LOGI("Total channels: %d", info_country.nchan);
    LOGI("Policy: %s", info_country.policy != WIFI_COUNTRY_POLICY_AUTO ? "manual" : "auto");
}

// Событие появления сетевого интерфейса
static os_event_group_t wifi_intf_event;

// По подключению к чужой точке
#define WIFI_INTF_EVENT_STATION     MASK_32(1, 0)
// По созданию своей точки
#define WIFI_INTF_EVENT_SOFTAP      MASK_32(1, 1)

// Обработчик событий ядра WiFi
static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    const system_event_info_t *info = &event->event_info;

    switch (event->event_id)
    {
        // --- Station --- //

        case SYSTEM_EVENT_STA_START:
            LOGI("[Station] started...");
            wifi_intf_event.set(WIFI_INTF_EVENT_STATION);
            break;
        case SYSTEM_EVENT_STA_STOP:
            LOGI("[Station] stopped...");
            wifi_intf_event.reset(WIFI_INTF_EVENT_STATION);
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            LOGI("[Station] got IP: %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            LOGI("[Station] disconnected, reason: %d", info->disconnected.reason);
            {
                auto result = esp_wifi_connect();
                if (result != ESP_ERR_WIFI_MODE)
                    ESP_ERROR_CHECK(result);
            }
            break;

        // --- Softap --- //

        case SYSTEM_EVENT_AP_START:
            LOGI("[Softap] started...");
            wifi_intf_event.set(WIFI_INTF_EVENT_SOFTAP);
            break;
        case SYSTEM_EVENT_AP_STOP:
            LOGI("[Softap] stopped...");
            wifi_intf_event.reset(WIFI_INTF_EVENT_SOFTAP);
            break;

        case SYSTEM_EVENT_AP_STACONNECTED:
            LOGI("[Softap] Station: " MACSTR " join", MAC2STR(event->event_info.sta_connected.mac));
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            LOGI("[Softap] Station: " MACSTR "leave", MAC2STR(event->event_info.sta_disconnected.mac));
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Промежуточный буфер для настроек
static wifi_settings_t wifi_settings;
// Структура хранения изменений по блокам настроек
static struct
{
    // Своя точка доступа
    bool softap;
    // Подключение к точке доступа
    bool station;
} wifi_settings_changed;

void wifi_command_handler_settings_get_t::notify(ipc_dir_t dir)
{
    // Мы можем только обрабатывать ответ
    assert(dir == IPC_DIR_RESPONSE);
    LOGI("Settings fetched!");
    state = STATE_IDLE;
    core_main_task.mutex.enter();
        // Проверка изменений
        wifi_settings_changed.softap = wifi_settings.softap != command.response.settings.softap;
        wifi_settings_changed.station = wifi_settings.station != command.response.settings.station;
        // Копирование настроек в промежуточный буфер
        wifi_settings = command.response.settings;
    core_main_task.mutex.leave();
    // Отложенный вызов пересоздания точек доступа
    core_main_task.deffered_call([](void)
    {
        core_main_task.mutex.enter();
            // Выбор интерфейсов
            wifi_mode_t mode = WIFI_MODE_NULL;
            if (wifi_settings.softap.use && wifi_settings.station.use)
                mode = WIFI_MODE_APSTA;
            else if (wifi_settings.softap.use)
                mode = WIFI_MODE_AP;
            else if (wifi_settings.station.use)
                mode = WIFI_MODE_STA;
            // Активация интерфейсов
            wifi_mode_t mode_current;
            ESP_ERROR_CHECK(esp_wifi_get_mode(&mode_current));
            if (mode_current != mode)
                ESP_ERROR_CHECK(esp_wifi_set_mode(mode));

            // Конфигурирование
            wifi_config_t config;
            // Точка доступа
            if (wifi_settings.softap.use && wifi_settings_changed.softap)
            {
                memset(&config.ap, 0, sizeof(config.ap));
                // Имя, пароль
                strcpy((char *)config.ap.ssid, wifi_settings.softap.ssid);
                strcpy((char *)config.ap.password, wifi_settings.softap.password);
                // Авторизация
                if (strlen(wifi_settings.softap.password) > 0)
                    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
                // Разное
                config.ap.max_connection = 4;
                config.ap.beacon_interval = 100;
                // Применение
                ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &config));
                // Лог
                LOGI("Creating our AP \"%s\"...", wifi_settings.softap.ssid);
            }

            // Подключение к точке
            if (wifi_settings.station.use && wifi_settings_changed.station)
            {
                memset(&config.sta, 0, sizeof(config.sta));
                // Имя, пароль
                strcpy((char *)config.sta.ssid, wifi_settings.station.ssid);
                strcpy((char *)config.sta.password, wifi_settings.station.password);
                // Применение
                ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &config));
                // Лог
                LOGI("Connecting to AP \"%s\"...", wifi_settings.station.ssid);
                ESP_ERROR_CHECK(esp_wifi_connect());
            }
        core_main_task.mutex.leave();
    });
}

// Параметры инициализации подсистемы WiFi
static const wifi_init_config_t WIFI_INIT_CONFIG = WIFI_INIT_CONFIG_DEFAULT();

void wifi_init(void)
{
    tcpip_adapter_init();
    // Отчистка промежуточного буфера настроек
    wifi_settings.clear();
    // Обработчик событий ядра WiFi
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    // Инициализация подсистемы WiFi
    ESP_ERROR_CHECK(esp_wifi_init(&WIFI_INIT_CONFIG));
    // Вся конфигурация в ОЗУ
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // Вывод информации о региональных настройках
    wifi_log_country_info();
    // Вывод MAC адресов
    wifi_log_mac("Station", WIFI_IF_STA);
    wifi_log_mac("Soft-AP", WIFI_IF_AP);
    // Отключение интерфейсов, старт
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    stm_add_event_handler(wifi_ipc_events);
    core_add_command_handler(wifi_command_handler_settings_get);
    core_add_command_handler(wifi_command_handler_settings_changed);
}

bool wifi_wait(os_tick_t ticks)
{
    return wifi_intf_event.wait(WIFI_INTF_EVENT_STATION | WIFI_INTF_EVENT_SOFTAP, false, ticks);
}
