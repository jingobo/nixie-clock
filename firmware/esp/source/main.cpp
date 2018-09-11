#include "io.h"
#include "core.h"
#include "ntime.h"

static void ROM wifi_station_config_init(station_config *ref)
{
    ref->bssid_set = false;
    strcpy((char *)ref->ssid, "Holiday!");
    strcpy((char *)ref->password, "02701890");
}

static bool ROM wifi_station_config_compare(const station_config *cfg1, const station_config *cfg2)
{
    return !strcmp((char *)cfg1->ssid, (char *)cfg2->ssid) &&
           !strcmp((char *)cfg1->password, (char *)cfg2->password) &&
           !memcmp(cfg1->bssid, cfg2->bssid, 6) &&
           cfg1->bssid_set == cfg2->bssid_set;
}

void ROM wifi_station_init(void)
{
    bool result;
    // Установка режима работы
    log_line("Check operation mode...");
    uint8_t mode = wifi_get_opmode();
    if (mode != STATION_MODE)
    {
        log_raw("Changing operation mode to station...");
        result = wifi_set_opmode(STATION_MODE);
        log_line(result ? "DONE!" : "FAILED!");
        if (!result)
            return;
    }
    wifi_station_disconnect();
    wifi_station_dhcpc_stop();
    // Установка параметров подключаемой сети
    station_config current_config;
    do
    {
        station_config flash_config;
        // Получаем текущие параметры подключения к WiFi
        if (wifi_station_get_config(&flash_config))
        {
            memcpy(&current_config, &flash_config, sizeof(station_config));
            wifi_station_config_init(&current_config);
            if (wifi_station_config_compare(&flash_config, &current_config))
            {
                log_line("Configuration of WiFi station is same!");
                break;
            }
        }
        else
        {
            log_line("Warning! Unable to retrieve configuration of WiFi station!");
            wifi_station_config_init(&current_config);
            memset(&current_config.bssid, 0, sizeof(current_config.bssid));
        }
        // Не удалось получить текущие настройки или они различаются...
        log_raw("Configuration of WiFi station override...");
        //ETS_UART_INTR_DISABLE();
            result = wifi_station_set_config(&current_config);
        //ETS_UART_INTR_ENABLE();
        if (result)
        {
            log_line("DONE!");
            break;
        }
        log_line("FAIL!");
        return;
    } while (false);
    // Подключение к выбранной сети
    //ETS_UART_INTR_DISABLE();
        result = wifi_station_connect();
    //ETS_UART_INTR_ENABLE();
    if (!result)
    {
        log_line("Fatal error! Unable to begin WiFi station connect!");
        return;
    }
    log_line("Connecting to %s...", current_config.ssid);
    wifi_station_dhcpc_start();
}


ROM void main_init(void)
{
    // Инициализация модулей
    io_init();
    core_init();
    ntime_init();
    // Инициализация WiFi (тест)
    wifi_station_init();
}
