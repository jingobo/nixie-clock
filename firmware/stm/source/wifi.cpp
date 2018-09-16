#include "wifi.h"

#include "esp.h"
#include "storage.h"
#include <proto/wifi.inc>

// Настройки WIFI
static wifi_settings_t wifi_settings @ STORAGE_SECTION =
{
    .softap =
    {
        .use = IPC_BOOL_FALSE,
        .channel = 1,
        .ssid = "NixieClock",
        .password = "",
    },
    .station =
    {
        .use = IPC_BOOL_TRUE,
        .ssid = "Holiday!",
        .password = "02701890"
    }
};

// Класс обслуживания WIFI
static class wifi_t : ipc_handler_event_t
{
    // Обработчик команды запроса настроек WIFI
    class handler_command_settings_t : public ipc_handler_command_template_t<wifi_command_settings_require_t>
    {
        // Ссылка на родительский объект
        wifi_t &wifi;
        // Состояние отправки
        bool sending;
        // Команда установки настроек WIFI
        wifi_command_settings_t command;
    protected:
        // Оповещение о поступлении данных
        virtual void notify(ipc_dir_t dir)
        {
            // Мы можем только обрабатывать запрос
            assert(dir == IPC_DIR_REQUEST);
            // Заполняем ответ
            command.request.settings = wifi_settings;
            // Указываем что нужно отправить ответ
            sending = true;
        }
    public:
        // Конструктор по умолчанию
        handler_command_settings_t(wifi_t &parent) : wifi(parent), sending(false)
        { }
        
        // Обработчик простоя
        void idle(void)
        {
            if (sending)
                sending = !esp_transmit(IPC_DIR_REQUEST, command);
        }
    } handler_command_settings;
protected:
    // Событие простоя
    virtual void idle(void)
    {
        // Базовый метод
        ipc_handler_event_t::idle();
        // Если нужно отправить настройки
        handler_command_settings.idle();
    }
public:
    // Конструктор по умолчанию
    wifi_t(void) : handler_command_settings(*this)
    { }
    
    // Инициализация
    INLINE_FORCED
    void init(void)
    {
        esp_handler_add_event(*this);
        esp_handler_add_command(handler_command_settings);
    }
} wifi;

volatile bool t;

void wifi_init(void)
{
    wifi.init();
}
