#include "esp.h"
#include "rtc.h" // TODO: выпил
#include "wifi.h"
#include "storage.h"
#include <proto/wifi.inc>

// Настройки WiFi
static wifi_settings_t wifi_settings @ STORAGE_SECTION =
{
    .softap =
    {
        .use = IPC_BOOL_FALSE,
        .ssid = "NixieClock",
        .password = "",
    },
    .station =
    {
        .use = IPC_BOOL_TRUE,
        .ssid = "Holyday! [2.4 GHz]",
        .password = "02701890"
    }
};

// Обработчик команды запроса настроек WiFi
static class wifi_handler_command_settings_get_t : public ipc_command_handler_template_sticky_t<wifi_command_settings_get_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать запрос
        assert(dir == IPC_DIR_REQUEST);
        // Заполняем ответ
        command.response.settings = wifi_settings;
        // Передача ответа
        transmit();
    }

    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        return esp_transmit(IPC_DIR_RESPONSE, command);
    }
} wifi_handler_command_settings_get;

// Обработчик команды оповещения о смене настроек WiFi
static class wifi_handler_command_settings_changed_t : public ipc_command_handler_template_sticky_t<wifi_command_settings_changed_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать ответ
        assert(dir == IPC_DIR_RESPONSE);
        __no_operation();
    }

    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        return esp_transmit(IPC_DIR_REQUEST, command);
    }
} wifi_handler_command_settings_changed;

// Обработчик событий IPC
static class wifi_handler_ipc_event_t : public ipc_event_handler_t
{
    // Обработчик события сброса
    virtual void reset(void)
    {
        wifi_handler_command_settings_get.reset();
        wifi_handler_command_settings_changed.reset();
    }

    // Обработчик события простоя
    virtual void idle(void)
    {
        wifi_handler_command_settings_get.idle();
        wifi_handler_command_settings_changed.idle();
    }
} wifi_handler_ipc_event;

static uint8_t wifi_test_time = 0;

// Событие наступления секунды
static callback_list_item_t wifi_second_event([](void)
{
    // TODO: выпилить
    /*if (++wifi_test_time == 10)
    {
        strcpy(wifi_settings.station.ssid, "Vitaly");
        strcpy(wifi_settings.station.password, "11111111");
        wifi_settings_changed();
        return;
    }
    if (wifi_test_time == 15)
    {
        wifi_settings.station.use = IPC_BOOL_FALSE;
        wifi_settings_changed();
        return;
    }
    if (wifi_test_time == 20)
    {
        wifi_settings.station.use = IPC_BOOL_TRUE;
        strcpy(wifi_settings.station.ssid, "Holyday! [2.4 GHz]");
        strcpy(wifi_settings.station.password, "02701890");
        wifi_settings_changed();
        return;
    }*/
});


void wifi_init(void)
{
    esp_add_event_handler(wifi_handler_ipc_event);
    esp_add_command_handler(wifi_handler_command_settings_get);
    esp_add_command_handler(wifi_handler_command_settings_changed);
    // Добавление обработчика секундного события
    rtc_second_event_add(wifi_second_event);
}

bool wifi_has_internet_get(void)
{
    return wifi_settings.station.use != IPC_BOOL_FALSE;
}

void wifi_settings_changed(void)
{
    wifi_handler_command_settings_changed.transmit();
}
