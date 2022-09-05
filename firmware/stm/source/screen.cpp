#include "esp.h"
#include "rtc.h"
#include "timer.h"
#include "screen.h"
#include "proto/screen.inc"

// Предварительное объявление
class screen_nixie_source_hook_t;

// Обработчик команды получения состояния экрана
static class screen_command_handler_state_get_t : public ipc_responder_template_t<screen_command_state_get_t>
{
    friend class screen_nixie_source_hook_t;
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

// Перехватчик данных источника ламп
static class screen_nixie_source_hook_t : public nixie_model_t::source_hook_t
{
protected:
    // Оповещение о сохранении данных
    virtual void data_store(hmi_rank_t index, const nixie_data_t &data) override final
    {
        screen_command_handler_state_get.command.response.nixie[index] = data;
    }
} screen_nixie_source_hook;

// Перехватчик данных источника светодиодов
static class screen_led_source_hook_t : public led_model_t::source_hook_t
{
protected:
    // Оповещение о сохранении данных
    virtual void data_store(hmi_rank_t index, const hmi_rgb_t &data) override final
    {
        screen_command_handler_state_get.command.response.led[index] = data;
    }
} screen_led_source_hook;

// Перехватчик данных источника неонок
static class screen_neon_source_hook_t : public neon_model_t::source_hook_t
{
protected:
    // Оповещение о сохранении данных
    virtual void data_store(hmi_rank_t index, const neon_data_t &data) override final
    {
        screen_command_handler_state_get.command.response.neon[index] = data;
    }
} screen_neon_source_hook;

// Экран
screen_t screen;

// Таймер обновления экрана
static timer_callback_t screen_refresh_timer([](void)
{
    screen.refresh();
});

// Событие наступления секунды
static callback_list_item_t screen_second_event([](void)
{
    screen.second();
});

void screen_init(void)
{
    // Добавление перехватчиков
    screen.led.attach(screen_led_source_hook);
    screen.neon.attach(screen_neon_source_hook);
    screen.nixie.attach(screen_nixie_source_hook);
    
    // Добавление обработчика секундного события
    rtc_second_event_add(screen_second_event);
    
    // Запуск задания на обновление экрана
    screen_refresh_timer.start_hz(HMI_FRAME_RATE, TIMER_PRI_HIGHEST | TIMER_FLAG_LOOP);
    // Обновление дисплея
    screen_refresh_timer.raise();
    
    // Обработчики IPC
    esp_handler_add(screen_command_handler_state_get);
}
