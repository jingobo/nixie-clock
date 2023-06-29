#include "esp.h"
#include "rtc.h"
#include "timer.h"
#include "screen.h"
#include "proto/screen.inc.h"

// Предварительное объявление
class screen_nixie_capture_t;

// Обработчик команды получения состояния экрана
static class screen_command_handler_state_get_t : public ipc_responder_template_t<screen_command_state_get_t>
{
    friend class screen_nixie_capture_t;
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;

        // Передача (данные уже готовы)
        transmit();
    }
} screen_command_handler_state_get;

// Класс захвата источника данных
template <typename MODEL>
class screen_data_capture_t : public MODEL::transceiver_t
{
    // Псевдонимы
    using model_t = MODEL;
    using data_t = typename model_t::data_t;
    using data_block_t = typename model_t::data_block_t;
    
    // Данные к установке
    data_block_t& dest;
public:
    // Основной конструктор
    screen_data_capture_t(data_block_t& _dest) : dest(_dest)
    { }
protected:
    // Получает, можно ли слой переносить в другую модель
    virtual bool moveable_get(void) const override final
    {
        // Перехватчика источника перемещать нельзя
        return false;
    }
    
    // Получает приоритет слоя
    virtual uint8_t priority_get(void) const override final
    {
        // Низший приоритет
        return model_t::PRIORITY_CAPTURE;
    }
        
    // Обработчик изменения данных
    virtual void data_changed(hmi_rank_t index, data_t &data) override final
    {
        dest[index] = data;
        // По умолчанию передача дальше
        model_t::transceiver_t::out_set(index, data);
    }
};

// Захват данных светодиодов
static screen_data_capture_t<led_model_t> screen_led_capture(screen_command_handler_state_get.command.response.led);
// Захват данных неонок
static screen_data_capture_t<neon_model_t> screen_neon_capture(screen_command_handler_state_get.command.response.neon);
// Захват данных ламп
static screen_data_capture_t<nixie_model_t> screen_nixie_capture(screen_command_handler_state_get.command.response.nixie);

// Экран
screen_t screen;

// Событие наступления секунды
static list_handler_item_t screen_second_event([](void)
{
    screen.second();
});

void screen_init(void)
{
    // Добавление перехватчиков
    screen.led.attach(screen_led_capture);
    screen.neon.attach(screen_neon_capture);
    screen.nixie.attach(screen_nixie_capture);
    
    // Добавление обработчика секундного события
    rtc_second_event_add(screen_second_event);

    // Обработчики IPC
    esp_handler_add(screen_command_handler_state_get);
}
