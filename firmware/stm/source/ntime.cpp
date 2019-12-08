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
static class ntime_command_handler_time_sync_t : public ipc_command_handler_template_t<time_command_sync_t>
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
        if (state == STATE_RESPONSE)
            state = STATE_REQUEST;
    }
    
    // Простой
    void idle(void)
    {
        if (state == STATE_REQUEST && esp_transmit(IPC_DIR_REQUEST, command))
            // Ожидаем ответ
            state = STATE_RESPONSE;
    }

    // Старт синхронизации
    void go(bool forced)
    {
        // При форсированном режиме синхронизация не замедлительно кроме - если уже запросили
        if (forced && state == STATE_RESPONSE)
            return;
        // При обычном режиме синхронизация только если сейчас в простое
        if (!forced && state > STATE_SUCCESS)
            return;
        // Проверяем, есть ли гепотетически интернет и можем ли синхронизировать
        if (wifi_has_internet_get() && ntime_sync_settings.can_sync())
            state = STATE_REQUEST;
    }

    // Секундное событие
    void second(void)
    {
        // Задержка запроса даты/времени
        if (state > STATE_REQUEST)
            state = ENUM_VALUE_PREV(state);
    }
} ntime_command_handler_time_sync;

// Обработчик команды запроса списка SNTP хостов
static class ntime_command_handler_hostlist_set_t : public ipc_command_handler_template_sticky_t<time_command_hostlist_set_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать ответ
        assert(dir == IPC_DIR_RESPONSE);
        // Форсированный запуск синхронизации
        ntime_command_handler_time_sync.go(true);
    }
    
    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        // Заполняем ответ
        time_hosts_data_copy(command.request.hosts, ntime_sync_settings.hosts);
        // Передачем
        return esp_transmit(IPC_DIR_REQUEST, command);
    }
} ntime_command_handler_hostlist_set;

void ntime_command_handler_time_sync_t::notify(ipc_dir_t dir)
{
    // Мы можем только обрабатывать ответ
    assert(dir == IPC_DIR_RESPONSE);
    // Если ошибка, переспросим
    switch (command.response.status)
    {
        case time_command_sync_response_t::STATUS_SUCCESS:
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
        case time_command_sync_response_t::STATUS_FAILED:
            // Возможно ошибка сети
            if (!wifi_has_internet_get())
            {
                state = STATE_SUCCESS;
                break;
            }
            // Сетевая ошибка, перезапрос через задержку
            state = STATE_NEEDED_DELAY_2;
            break;
        case time_command_sync_response_t::STATUS_HOSTLIST:
            // Нет хостов, отаправляем
            if (time_hosts_empty(ntime_sync_settings.hosts))
            {
                state = STATE_SUCCESS;
                break;
            }
            ntime_command_handler_hostlist_set.transmit();
            state = STATE_NEEDED_DELAY_4;
            break;
        default:
            assert(false);
            break;
    }
}

// Обработчик команды запроса текущей даты/времени
static class ntime_command_handler_current_get_t : public ipc_command_handler_template_sticky_t<time_command_current_get_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать запрос
        assert(dir == IPC_DIR_REQUEST);
        // Передача
        transmit();
    }
    
    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        // Заполняем ответ
        rtc_datetime_get(command.response);
        // Передачем
        return esp_transmit(IPC_DIR_RESPONSE, command);
    }
} ntime_command_handler_current_get;

// Обработчик команды установки текущей даты/времени
static class ntime_command_handler_current_set_t : public ipc_command_handler_template_sticky_t<time_command_current_set_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать запрос
        assert(dir == IPC_DIR_REQUEST);
        // Установка даты/времени
        rtc_datetime_set(command.request);
        // Передача
        transmit();
    }
    
    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        // Передачем
        return esp_transmit(IPC_DIR_RESPONSE, command);
    }
} ntime_command_handler_current_set;

// Обработчик команды запроса настроек даты/времени
static class ntime_command_handler_settings_get_t : public ipc_command_handler_template_sticky_t<time_command_settings_get_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать запрос
        assert(dir == IPC_DIR_REQUEST);
        // Передача
        transmit();
    }
    
    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        // Заполняем ответ
        command.response = ntime_sync_settings;
        // Передачем
        return esp_transmit(IPC_DIR_RESPONSE, command);
    }
} ntime_command_handler_settings_get;

// Обработчик команды установки настроек даты/времени
static class ntime_command_handler_settings_set_t : public ipc_command_handler_template_sticky_t<time_command_settings_set_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать запрос
        assert(dir == IPC_DIR_REQUEST);
        // Установка настроек
        ntime_sync_settings = command.request;
        storage_modified();
        // Передача
        transmit();
    }
    
    // Обработчик передачи
    virtual bool transmit_internal(void)
    {
        // Передачем
        return esp_transmit(IPC_DIR_RESPONSE, command);
    }
} ntime_command_handler_settings_set;

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
        ntime_command_handler_time_sync.idle();
        ntime_command_handler_current_get.idle();
        ntime_command_handler_current_set.idle();
        ntime_command_handler_settings_get.idle();
        ntime_command_handler_settings_set.idle();
    }
    
    // Событие сброса
    virtual void reset(void)
    {
        // Базовый метод
        ipc_event_handler_t::reset();
        // Команды
        ntime_command_handler_time_sync.reset();
        ntime_command_handler_current_get.reset();
        ntime_command_handler_current_set.reset();
        ntime_command_handler_hostlist_set.reset();
        ntime_command_handler_settings_get.reset();
        ntime_command_handler_settings_set.reset();
    }
} ntime_ipc_handler;

// Обработчик события инкремента секунды
static callback_list_item_t ntime_second_event([](void)
{
    // Оповещение о секунде
    ntime_command_handler_time_sync.second();
});

void ntime_init(void)
{
    // Секундный обработчик
    rtc_second_event_add(ntime_second_event);
    // Обработчики IPC
    esp_add_event_handler(ntime_ipc_handler);
    esp_add_command_handler(ntime_command_handler_time_sync);
    esp_add_command_handler(ntime_command_handler_current_get);
    esp_add_command_handler(ntime_command_handler_current_set);
    esp_add_command_handler(ntime_command_handler_settings_get);
    esp_add_command_handler(ntime_command_handler_settings_set);
    esp_add_command_handler(ntime_command_handler_hostlist_set);
    // Запуск синхронизации
    ntime_sync();
}

void ntime_sync(void)
{
    ntime_command_handler_time_sync.go(false);
}
