#include "esp.h"
#include "wifi.h"
#include "storage.h"
#include <proto/wifi.inc>

// Настройки
static wifi_settings_t wifi_settings @ STORAGE_SECTION =
{
    .softap =
    {
        .use = IPC_BOOL_TRUE,
        .ssid = "NixieClock",
        .password = "",
    },
    .station =
    {
        .use = IPC_BOOL_TRUE,
        .ssid = "Vidikon45",
        .password = "89156137267"
    }
};

// Обработчик команды оповещения о смене настроек
static class wifi_command_handler_settings_changed_t : public ipc_requester_template_t<wifi_command_settings_changed_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        // Ничего не делаем
    }
public:
    // Оповещение о смене настроек
    void transmit(void)
    {
        ipc_requester_template_t::transmit();
    }
} wifi_command_handler_settings_changed;

// Обработчик команды чтения настроек
static class wifi_command_handler_settings_get_t : public ipc_responder_template_t<wifi_command_settings_get_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // Заполняем ответ
        command.response = wifi_settings;
        // Передача ответа
        transmit();
    }
} wifi_command_handler_settings_get;

// Обработчик команды записи настроек
static class wifi_command_handler_settings_set_t : public ipc_responder_template_t<wifi_command_settings_set_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // Чтение данных
        wifi_settings = command.request;
        storage_modified();
        // Передача ответа
        transmit();
        // Оповещение о изменении настроек
        wifi_command_handler_settings_changed.transmit();
    }
} wifi_command_handler_settings_set;

void wifi_init(void)
{
    esp_handler_add(wifi_command_handler_settings_get);
    esp_handler_add(wifi_command_handler_settings_set);
    esp_handler_add(wifi_command_handler_settings_changed);
}

bool wifi_has_internet_get(void)
{
    return wifi_settings.station.use != IPC_BOOL_FALSE;
}
