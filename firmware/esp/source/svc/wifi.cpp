#include <stm.h>
#include <log.h>
#include <core.h>
#include "wifi.h"
#include <tool.h>
#include <system.h>
#include <esp_wifi.h>
#include <tcpip_adapter.h>
#include <esp_event_loop.h>

#include <proto/wifi.inc>

// Имя модуля для логирования
LOG_TAG_DECL("WIFI");

// Промежуточный буфер для настроек
static wifi_settings_t wifi_settings;
// Состояние сети
static wifi_intf_states_t wifi_state;

// Класс поиска сетей
static class wifi_net_finder_t
{
    // Количество точек
    uint16_t count = 0;
    // Признак запуска
    bool running = false;
public:
    // Мьютекс для синхронизации
    const os_mutex_t mutex;
    // Список точек
    wifi_ap_record_t ap_records[wifi_command_search_pool_request_t::MAX_INDEX];

    // Конструктор по умолчанию
    wifi_net_finder_t(void)
    { }

    // Запуск поиска
    void start(void)
    {
    	mutex.enter();
			if (running)
			{
				mutex.leave();
				return;
			}
			running = true;
			count = 0;
    	mutex.leave();

        LOGI("[Finder] started...");

    	wifi_scan_config_t config;
    	memset(&config, 0, sizeof(config));
    	config.scan_time.active.min = 100;
		config.scan_time.active.max = 500;

    	ESP_ERROR_CHECK(esp_wifi_scan_start(&config, false));
    }

    // Обработчик завершения
    void done(void)
    {
    	mutex.enter();
    		assert(running);

    		// Получаем точки
    		count = ARRAY_SIZE(ap_records);
    		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&count, ap_records));

    		// Снимаем признак
    		running = false;

    		// Логирование
            LOGI("[Finder] done. Found %d aps...", count);

            for (auto i = 0; i < count; i++)
            {
            	// Поиск сетки к которой подключены
            	const auto &ap = ap_records[i];

            	// Лог
                LOGI("[Finder] ap \"%s\", rssi %d, private %s.",
                	ap.ssid,
					ap.rssi,
					ap.authmode != WIFI_AUTH_OPEN ?
						"yes" :
						"no");
            }

    	mutex.leave();
    }

    // Получает признак запуска
    bool running_get(void) const
    {
    	return running;
    }

    // Получает количество найденных сетей
    uint8_t count_get(void) const
    {
    	return count;
    }

} wifi_net_finder;

// Таймер переподключения к точке доступа
static os_timer_t wifi_station_reconnect_timer;

// Переподключение к точке доступа
static void wifi_station_reconnect(void)
{
	wifi_state.station = WIFI_INTF_STATE_ESTABLISH;
    os_timer_disarm(&wifi_station_reconnect_timer);
    ESP_ERROR_CHECK(esp_wifi_connect());
}

// Обработчик переподключения к точке доступа
static void wifi_station_reconnect_handler(void *arg)
{
	UNUSED(arg);

	// Выход если подключение отключено
	if (!wifi_settings.station.use)
		return;

	// Выход если запущен поиск
	if (wifi_net_finder.running_get())
		return;

	// Проверка состояния
	if (wifi_state.station != WIFI_INTF_STATE_ERROR)
		return;

	// Переподключение
    LOGI("[Station] reconnecting...");
    wifi_station_reconnect();
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
    const auto &info = event->event_info;

    switch (event->event_id)
    {
    	// --- Shared --- //

    	case SYSTEM_EVENT_SCAN_DONE:
    		wifi_net_finder.done();
    		break;

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
            LOGI("[Station] got IP: %s", ip4addr_ntoa(&info.got_ip.ip_info.ip));
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            LOGI("[Station] lost IP!");
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            LOGI("[Station] connected, channel: %d", info.connected.channel);
            wifi_state.station = WIFI_INTF_STATE_READY;
            os_timer_disarm(&wifi_station_reconnect_timer);
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            LOGI("[Station] disconnected, reason: %d", info.disconnected.reason);
            if (wifi_settings.station.use)
            {
                wifi_state.station = WIFI_INTF_STATE_ERROR;
            	os_timer_arm(&wifi_station_reconnect_timer, 10000, true);
            }
            break;

        // --- Softap --- //

        case SYSTEM_EVENT_AP_START:
            LOGI("[Softap] started...");
            wifi_state.softap = WIFI_INTF_STATE_READY;
            wifi_intf_event.set(WIFI_INTF_EVENT_SOFTAP);
            break;
        case SYSTEM_EVENT_AP_STOP:
            LOGI("[Softap] stopped...");
            wifi_state.softap = WIFI_INTF_STATE_IDLE;
            wifi_intf_event.reset(WIFI_INTF_EVENT_SOFTAP);
            break;

        case SYSTEM_EVENT_AP_STACONNECTED:
            LOGI("[Softap] Station: " MACSTR " join", MAC2STR(info.sta_connected.mac));
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            LOGI("[Softap] Station: " MACSTR "leave", MAC2STR(info.sta_disconnected.mac));
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
        	// Нет деталей
            break;

        default:
            LOGW("Unhandled event, code: %d", event->event_id);
            break;
    }

    return ESP_OK;
}

// Обработчик команды запроса настроек WIFI
static class wifi_command_handler_settings_get_t : public ipc_requester_template_t<wifi_command_settings_get_t>
{
    // Состояние запроса
    bool requesting = true;
protected:
    // Обработка данных
    virtual void work(bool idle) override final
    {
        if (idle)
        {
            if (requesting)
            {
                LOGI("Settings requested...");
                transmit();
            }
            return;
        }

        // Настройки получены
        requesting = false;
        LOGI("Settings fetched!");

        /* Применение настроек в отдельной задаче бессмысленно так как все
         * задачи останавливаются на момент применения! */

        // Проверка изменений
        const auto softap_changed = wifi_settings.softap != command.response.softap;
        const auto station_changed = wifi_settings.station != command.response.station;
        // Копирование настроек в промежуточный буфер
        wifi_settings = command.response;

        // Изменение интерфейса
        {
            // Выбор интерфейсов
            const wifi_mode_t mode = wifi_settings.softap.use ?
            		WIFI_MODE_APSTA :
					WIFI_MODE_STA;

            // Активация интерфейсов
            wifi_mode_t mode_current;
            ESP_ERROR_CHECK(esp_wifi_get_mode(&mode_current));
            if (mode_current != mode)
                ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
        }

        // Конфигурирование
        wifi_config_t config;

        // Точка доступа
        if (wifi_settings.softap.use && softap_changed)
        {
            memset(&config.ap, 0, sizeof(config.ap));
            // Имя, пароль
            memcpy(config.ap.ssid, wifi_settings.softap.ssid, sizeof(wifi_ssid_t));
            memcpy(config.ap.password, wifi_settings.softap.password, sizeof(wifi_password_t));
            // Авторизация
            if (strlen(wifi_settings.softap.password) > 0)
                config.ap.authmode = WIFI_AUTH_WPA2_PSK;
            // Канал
            config.ap.channel = wifi_settings.softap.channel;
            // Разное
            config.ap.max_connection = 4;
            config.ap.beacon_interval = 100;
            // Применение
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &config));
            LOGI("Creating our AP \"%s\"...", wifi_settings.softap.ssid);
        }

        // Подключение к точке
        if (station_changed)
        {
        	// Сначала отключение
            wifi_state.station = WIFI_INTF_STATE_IDLE;
			ESP_ERROR_CHECK(esp_wifi_disconnect());

			if (wifi_settings.station.use)
			{
				memset(&config.sta, 0, sizeof(config.sta));
				// Имя, пароль
				memcpy(config.sta.ssid, wifi_settings.station.ssid, sizeof(wifi_ssid_t));
				memcpy(config.sta.password, wifi_settings.station.password, sizeof(wifi_password_t));
				// Канал
				config.sta.channel = wifi_settings.station.channel;
				// Применение
				ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &config));
				LOGI("Connecting to AP \"%s\"...", wifi_settings.station.ssid);
			    wifi_station_reconnect();
			}
        }
    }
public:
    // Конструктор по умолчанию
    wifi_command_handler_settings_get_t(void) : ipc_requester_template_t(5000)
    { }

    // Обработчик события сброса
    void reset(void)
    {
        requesting = true;
    }
} wifi_command_handler_settings_get;

// Обработчик команды чтения состояния сети
static class wifi_command_handler_state_get_t : public ipc_responder_template_t<wifi_command_state_get_t>
{
protected:
    // Обработка данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;

        // Отправляем ответ
        command.response = wifi_state;
        transmit();
    }
} wifi_command_handler_state_get;

// Обработчик команды оповещения о смене настроек WiFi
static class wifi_command_handler_settings_changed_t : public ipc_responder_template_t<wifi_command_settings_changed_t>
{
protected:
    // Обработка данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // Перезапрашиваем настройки
        wifi_command_handler_settings_get.reset();
        // Отправляем ответ
        transmit();
        // Лог
        LOGI("Settings changed!");
    }
} wifi_command_handler_settings_changed;

// Обработчик команды поиска сети с опросом состояния
static class wifi_command_handler_search_pool_t : public ipc_responder_template_t<wifi_command_search_pool_t>
{
protected:
    // Обработка данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;

        // Отчистка ответа
        memset(&command.response, 0, sizeof(command.response));

        // Разбор команды
        switch (command.request.command)
        {
        	case wifi_command_search_pool_request_t::COMMAND_POOL:
        		// Если всё еще запущен
        		if (wifi_net_finder.running_get())
        		{
            		command.response.status = wifi_command_search_pool_response_t::STATUS_RUNNING;
        			break;
        		}

        		// Если всё отдали
        		if (command.request.index >= wifi_net_finder.count_get())
        		{
            		command.response.status = wifi_command_search_pool_response_t::STATUS_IDLE;
        			break;
        		}

        		command.response.status = wifi_command_search_pool_response_t::STATUS_RECORD;

        		// Записываем точку
        		{
        			auto &record = command.response.record;
        			const auto &ap = wifi_net_finder.ap_records[command.request.index];

        			memcpy(record.ssid, ap.ssid, sizeof(ap.ssid));
        			record.rssi = ap.rssi;
        			record.priv = (ap.authmode != WIFI_AUTH_OPEN) ?
        				IPC_BOOL_TRUE :
						IPC_BOOL_FALSE;
        		}
        		break;

        	case wifi_command_search_pool_request_t::COMMAND_START:
        		// В момент подключения нельзя запускать поиск
        		if (wifi_state.station == WIFI_INTF_STATE_ESTABLISH)
        		{
            		command.response.status = wifi_command_search_pool_response_t::STATUS_IDLE;
            		break;
        		}

        		// (Пере)запуск
        		wifi_net_finder.start();
        		command.response.status = wifi_command_search_pool_response_t::STATUS_RUNNING;
        		break;

        	default:
        		assert(false);
        }

        // Отправляем ответ
        transmit();
    }
} wifi_command_handler_search_pool;

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

void wifi_init(void)
{
	// Параметры инициализации подсистемы WiFi
	static const wifi_init_config_t INIT_CONFIG = WIFI_INIT_CONFIG_DEFAULT();

    tcpip_adapter_init();
    // Отчистка промежуточного буфера настроек
    wifi_settings.clear();
    // Обработчик событий ядра WiFi
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    // Инициализация подсистемы WiFi
    ESP_ERROR_CHECK(esp_wifi_init(&INIT_CONFIG));
    // Вся конфигурация в ОЗУ
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Вывод информации о региональных настройках
    wifi_log_country_info();

    // Вывод MAC адресов
    wifi_log_mac("Station", WIFI_IF_STA);
    wifi_log_mac("Soft-AP", WIFI_IF_AP);

    // Инициализация интерфейсов
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // Запрет автоподключения
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    //ESP_ERROR_CHECK(esp_wifi_set_auto_connect(false));
#pragma GCC diagnostic pop
    // Старт
    ESP_ERROR_CHECK(esp_wifi_start());

    // Добавление обработчиков IPC
    core_handler_add(wifi_command_handler_state_get);
    core_handler_add(wifi_command_handler_search_pool);
    core_handler_add(wifi_command_handler_settings_get);
    core_handler_add(wifi_command_handler_settings_changed);

    // Таймер переподключения к точке доступа
	MEMORY_CLEAR(wifi_station_reconnect_timer);
	os_timer_setfn(&wifi_station_reconnect_timer, wifi_station_reconnect_handler, NULL);
}

bool wifi_wait(os_tick_t ticks)
{
    return wifi_intf_event.wait(WIFI_INTF_EVENT_STATION | WIFI_INTF_EVENT_SOFTAP, false, ticks);
}
