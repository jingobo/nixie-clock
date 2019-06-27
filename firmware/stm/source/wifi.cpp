#include "esp.h"
#include "wifi.h"
#include "storage.h"
#include <proto/wifi.inc>

// Настройки WIFI
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
        .ssid = "Holyday! [2.4 GHz]",
        .password = "02701890"
    }
};

// Обработчик команды запроса настроек WIFI
class wifi_handler_command_settings_get_t : public ipc_command_handler_template_t<wifi_command_settings_get_t>
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
        esp_transmit(IPC_DIR_RESPONSE, command);
    }
} wifi_handler_command_settings_get;

void wifi_init(void)
{
    esp_add_command_handler(wifi_handler_command_settings_get);
}
