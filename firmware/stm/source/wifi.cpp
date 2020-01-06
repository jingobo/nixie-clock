#include "esp.h"
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
static class wifi_command_handler_settings_get_t : public ipc_responder_template_t<wifi_command_settings_get_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // Заполняем ответ
        command.response.settings = wifi_settings;
        // Передача ответа
        transmit();
    }
} wifi_command_handler_settings_get;

// Обработчик команды оповещения о смене настроек WiFi
static class wifi_command_handler_settings_changed_t : public ipc_requester_template_t<wifi_command_settings_changed_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        // Ничего не делаем
    }
public:
    // Оповещение о смене настроек WiFi
    void transmit(void)
    {
        ipc_requester_template_t::transmit();
    }
} wifi_command_handler_settings_changed;

void wifi_init(void)
{
    esp_handler_add(wifi_command_handler_settings_get);
    esp_handler_add(wifi_command_handler_settings_changed);
}

bool wifi_has_internet_get(void)
{
    return wifi_settings.station.use != IPC_BOOL_FALSE;
}

void wifi_settings_changed(void)
{
    wifi_command_handler_settings_changed.transmit();
}
