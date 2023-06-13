#include "esp.h"
#include "mcu.h"
#include "wifi.h"
#include "storage.h"
#include <proto/wifi.inc.h>

// Настройки
static wifi_settings_t wifi_settings @ STORAGE_SECTION =
{
    .intf =
    {
        // WIFI_INTF_STATION
        {
            .use = true,
            .ssid = "Vidikon45",
            .password = "89156137267"
        },
        
        // WIFI_INTF_SOFTAP
        {
            .use = true,
            .ssid = "NixieClock",
            .password = "",
        }
    }
};

// Время в мС времени изменения настроек
static __no_init uint32_t wifi_settings_time_change;

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

// Обработчик события изменения настроек
static void wifi_settings_time_changed(void)
{
    wifi_settings_time_change = mcu_tick_get();
    wifi_command_handler_settings_changed.transmit();
}

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
        wifi_settings_time_changed();
    }
} wifi_command_handler_settings_set;

#include "display.h"

// Обработчик команды репортирования о присвоении IP адреса
static class wifi_command_handler_ip_report_t : public ipc_responder_template_t<wifi_command_ip_report_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        
        // Показ если с момента изменения настроек не прошло 10 секунд
        if (mcu_tick_get() - wifi_settings_time_change < 10000)
            display_show_ip(command.request.intf, command.request.ip);
        
        // Передача ответа
        transmit();
    }
} wifi_command_handler_ip_report;

void wifi_init(void)
{
    esp_handler_add(wifi_command_handler_ip_report);
    esp_handler_add(wifi_command_handler_settings_get);
    esp_handler_add(wifi_command_handler_settings_set);
    esp_handler_add(wifi_command_handler_settings_changed);
    
    wifi_settings_time_changed();
}

bool wifi_has_internet_get(void)
{
    return wifi_settings.intf[WIFI_INTF_STATION].use;
}
