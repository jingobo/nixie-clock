#include "esp.h"
#include "wifi.h"
#include "storage.h"
#include <proto/wifi.inc>

// Настройки WiFi
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

// Обработчик команды запроса настроек WiFi
class wifi_handler_command_settings_get_t : public ipc_command_handler_template_sticky_t<wifi_command_settings_get_t>, public ipc_event_handler_t
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
    
    // Обработчик события сброса
    virtual void reset(void)
    {
        ipc_event_handler_t::reset();
        ipc_command_handler_template_sticky_t::reset();
    }

    // Обработчик события простоя
    virtual void idle(void)
    {
        ipc_event_handler_t::idle();
        ipc_command_handler_template_sticky_t::idle();
    }
} wifi_handler_command_settings_get;

void wifi_init(void)
{
    esp_add_event_handler(wifi_handler_command_settings_get);
    esp_add_command_handler(wifi_handler_command_settings_get);
}

bool wifi_has_internet_get(void)
{
    return wifi_settings.station.use != IPC_BOOL_FALSE;
}
