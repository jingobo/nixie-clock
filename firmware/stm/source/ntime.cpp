#include "rtc.h"
#include "esp.h"
#include "wifi.h"
#include "ntime.h"
#include "event.h"
#include "storage.h"
#include <proto/time.inc>

// Настройки синхронизации
static time_sync_settings_t ntime_sync_settings @ STORAGE_SECTION =
{
    // Синхронизация включена
    .sync = IPC_BOOL_TRUE,
    // UTC +03:00
    .timezone = 6,
    // Без смещения
    .offset = 0,
    // Раз в час
    .period = time_sync_settings_t::SYNC_PERIOD_HOUR,
    // Список хостов по умолчанию
    .hosts = "ntp1.stratum2.ru\n0.pool.ntp.org"
};

// Хранит время последней синхронизации
/*__no_init*/ static datetime_t ntime_sync_time;

// Обработчик команды получения даты/времени
static class ntime_command_time_get_t : public ipc_command_handler_template_t<time_command_get_t>
{
    // Состояние синхронизации
    enum
    {
        // Синхронизированно успешно
        STATE_SUCCESS = 0,
        // Ожидание завершения синхронизации
        STATE_RESPONSE,
        // Необходима синхронизация
        STATE_REQUEST,
        // Задержка синхронизации 1
        STATE_NEEDED_DELAY_1,
        // Задержка синхронизации 2
        STATE_NEEDED_DELAY_2,
        // Задержка синхронизации 3
        STATE_NEEDED_DELAY_3,
        // Задержка синхронизации 4
        STATE_NEEDED_DELAY_4
    } state = STATE_SUCCESS;
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir);
public:
    // Сброс
    void reset(void)
    {
        // Перезапрашиваем
        state = STATE_REQUEST;
    }

    // Старт синхронизации
    void sync(void)
    {
        // Перезапрашиваем если 
        if (state <= STATE_SUCCESS && wifi_has_internet_get() && ntime_sync_settings.can_sync())
            state = STATE_REQUEST;
    }
    
    // Простой
    void idle(void)
    {
        if (state == STATE_REQUEST && esp_transmit(IPC_DIR_REQUEST, command))
            // Ожидаем ответ
            state = STATE_RESPONSE;
    }
        
    // Секундное событие
    void second(void)
    {
        // Задержка запроса даты/времени
        if (state > STATE_REQUEST)
            state = ENUM_VALUE_PREV(state);
    }
} ntime_command_time_get;

// Обработчик команды запроса списка SNTP хостов
static class ntime_command_hostlist_set_t : public ipc_command_handler_template_sticky_t<time_command_hostlist_set_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать ответ
        assert(dir == IPC_DIR_RESPONSE);
        // Сброс
        reset();
    }
    
    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        // Заполняем ответ
        time_hosts_data_copy(command.request.hosts, ntime_sync_settings.hosts);
        // Передачем
        return esp_transmit(IPC_DIR_REQUEST, command);
    }
public:
    // Сброс
    virtual void reset(void)
    {
        // Базовый метод
        ipc_command_handler_template_sticky_t::reset();
        // Заодно сброс команды получения времени
        ntime_command_time_get.reset();
    }
} ntime_command_hostlist_set;

void ntime_command_time_get_t::notify(ipc_dir_t dir)
{
    // Мы можем только обрабатывать ответ
    assert(dir == IPC_DIR_RESPONSE);
    // Если ошибка, переспросим
    switch (command.response.status)
    {
        case time_command_get_response_t::STATUS_SUCCESS:
            // Получили дату
            state = STATE_SUCCESS;
            // Применение GMT
            {
                // Подсчет смещения часов
                auto temp = ntime_sync_settings.timezone / 2;
                temp += ntime_sync_settings.offset;
                command.response.value.shift_hour(temp);
                // Подсчет смещения минут
                temp = ntime_sync_settings.timezone % 2;
                if (temp > 0)
                    temp = 30;
                command.response.value.shift_minute(temp);
            }
            // Сохраняем время последней синхронизации
            ntime_sync_time = command.response.value;
            // Устанавилваем новое время
            if (ntime_sync_settings.can_sync())
                rtc_datetime_set(command.response.value);
            break;
        case time_command_get_response_t::STATUS_NETWORK:
            // Нет сети
            if (!wifi_has_internet_get())
            {
                state = STATE_SUCCESS;
                break;
            }
            // Нет опечатки
        case time_command_get_response_t::STATUS_FAILED:
            // Сетевая ошибка, перезапрос через задержку
            state = STATE_NEEDED_DELAY_2;
            break;
        case time_command_get_response_t::STATUS_HOSTLIST:
            // Нет хостов, отаправляем
            if (time_hosts_empty(ntime_sync_settings.hosts))
            {
                state = STATE_SUCCESS;
                break;
            }
            ntime_command_hostlist_set.transmit();
            state = STATE_NEEDED_DELAY_4;
            break;
        default:
            assert(false);
            break;
    }
}

// Класс обработчика событий IPC
static class ntime_ipc_handler_t : public ipc_event_handler_t
{
protected:
    // Событие простоя
    virtual void idle(void)
    {
        // Базовый метод
        ipc_event_handler_t::idle();
        // Команды
        ntime_command_time_get.idle();
    }
    
    // Событие сброса
    virtual void reset(void)
    {
        // Базовый метод
        ipc_event_handler_t::reset();
        // Команды
        ntime_command_hostlist_set.reset();
    }
} ntime_ipc_handler;

// Обработчик события инкремента секунды
static callback_list_item_t ntime_second_event([](void)
{
    // Оповещение о секунде
    ntime_command_time_get.second();
});

void ntime_init(void)
{
    // Секундный обработчик
    rtc_second_event_add(ntime_second_event);
    // Обработчики IPC
    esp_add_event_handler(ntime_ipc_handler);
    esp_add_command_handler(ntime_command_time_get);
    esp_add_command_handler(ntime_command_hostlist_set);
    // Запуск синхронизации
    ntime_sync();
}

void ntime_sync(void)
{
    ntime_command_time_get.sync();
}
