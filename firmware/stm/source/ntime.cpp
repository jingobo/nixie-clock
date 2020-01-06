#include "rtc.h"
#include "esp.h"
#include "wifi.h"
#include "ntime.h"
#include "storage.h"
#include <proto/time.inc>

// Настройки синхронизации
static time_sync_settings_t ntime_sync_settings @ STORAGE_SECTION =
{
    // Синхронизация включена
    .sync = IPC_BOOL_TRUE,
    // UTC +03:00
    .timezone = 6,
    // Стандартное время
    .offset = 0,
    // Список хостов по умолчанию
    .hosts = "ntp1.stratum2.ru\n0.pool.ntp.org"
};

// Хранит время последней синхронизации
__no_init static datetime_t ntime_sync_time;

// Отчистка даты последней синхронизации
static void ntime_sync_time_clear(void)
{
    memset(&ntime_sync_time, 0, sizeof(ntime_sync_time));
}

// Обработчик команды получения даты/времени
static class ntime_command_handler_time_sync_t : public ipc_requester_template_t<time_command_sync_t>
{
    // Количество попыток синхронизации
    uint8_t try_count = 0;
    // Таймер запроса
    timer_t request_timer;
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final;
public:
    // Конструктор по умолчанию
    ntime_command_handler_time_sync_t(void) : ipc_requester_template_t(15000)
    { }

    // Старт синхронизации
    bool go(void)
    {
        // Проверяем, есть ли гепотетически интернет и можем ли синхронизировать
        if (wifi_has_internet_get() && ntime_sync_settings.can_sync())
        {
            request_timer.start();
            try_count = 0;
            return true;
        }
        request_timer.stop();
        return false;
    }
} ntime_command_handler_time_sync;

// Обработчик команды запуска синхронизации даты/времени
static class ntime_command_handler_time_sync_start_t : public ipc_responder_template_t<time_command_sync_start_t>
{
    // Состояния запуска синхронизации
    bool running = false;
    // Результат операции
    bool success = false;
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        switch (command.request.action)
        {
            // Запуск
            case time_command_sync_start_request_t::ACTION_START:
                if (running)
                    break;
                // Запрос
                success = false;
                running = ntime_command_handler_time_sync.go();
                break;
            case time_command_sync_start_request_t::ACTION_CHECK:
                // Ничего не делаем
                break;
        }
        // Передача результата
        if (running)
            command.response.status = time_command_sync_start_response_t::STATUS_PENDING;
        else
            command.response.status = success ?
                time_command_sync_start_response_t::STATUS_SUCCESS :
                time_command_sync_start_response_t::STATUS_FAILED;
        transmit();
    }
public:
    // Репортирование о результате синхронизации
    void report(bool status)
    {
        running = false;
        success = status;
    }
} ntime_command_handler_time_sync_start;

// Обработчик команды запроса списка SNTP хостов
static class ntime_command_handler_hostlist_t : public ipc_requester_template_t<time_command_hostlist_set_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // При получении ответа - синхронизация
        ntime_command_handler_time_sync.go();
    }
public:
    /* Значение 2500 мС повтора связанно с тем что так попадают звезды и ответ
     * на эту команду совпадает с применением WiFi настроек, а там на ~2 сек
     * останавливаются все задачи */
    // Конструктор по умолчанию
    ntime_command_handler_hostlist_t(void) : ipc_requester_template_t(2500)
    { }
    
    // Передача списка
    bool upload(void)
    {
        // Если список не пуст
        if (time_hosts_empty(ntime_sync_settings.hosts))
            return false;
        // Подготовка списка
        time_hosts_data_copy(command.request.hosts, ntime_sync_settings.hosts);
        // Передача
        transmit();
        return true;
    }
} ntime_command_handler_hostlist;

void ntime_command_handler_time_sync_t::work(bool idle)
{
    if (idle)
    {
        // При простое определение передачи запроса
        if (request_timer.elapsed())
            transmit();
        return;
    }
    // Обработка ответа
    request_timer.stop();
    // Если ошибка, переспросим
    switch (command.response.status)
    {
        case time_command_sync_response_t::STATUS_SUCCESS:
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
            // Рапортирование автомату синхронизации
            ntime_command_handler_time_sync_start.report(true);
            break;
            
        case time_command_sync_response_t::STATUS_FAILED:
            // Если есть еще попытки и есть WiFi
            if (try_count > 10 || !wifi_has_internet_get())
            {
                // Рапортирование автомату синхронизации
                ntime_command_handler_time_sync_start.report(false);
                break;
            }
            // Возможно ошибка сети, перезапрос через 3 секунды
            try_count++;
            request_timer.start(3000);
            break;
            
        case time_command_sync_response_t::STATUS_HOSTLIST:
            // Нет хостов, отаправляем
            if (!ntime_command_handler_hostlist.upload())
                // Если список хостов отправить не удалось - на этом всё
                ntime_command_handler_time_sync_start.report(false);
            break;
        default:
            assert(false);
            break;
    }
}

// Обработчик команды запроса текущей даты/времени
static class ntime_command_handler_current_get_t : public ipc_responder_template_t<time_command_current_get_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // Заполняем ответ
        command.response.sync = ntime_sync_time;
        rtc_datetime_get(command.response.current);
        // Передача ответа
        transmit();
    }
} ntime_command_handler_current_get;

// Обработчик команды установки текущей даты/времени
static class ntime_command_handler_current_set_t : public ipc_responder_template_t<time_command_current_set_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // Установка даты/времени
        rtc_datetime_set(command.request);
        // Передача подтверждения
        transmit();
    }
} ntime_command_handler_current_set;

// Обработчик команды запроса настроек даты/времени
static class ntime_command_handler_settings_get_t : public ipc_responder_template_t<time_command_settings_get_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // Заполняем ответ
        command.response = ntime_sync_settings;
        // Передача ответа
        transmit();
    }
} ntime_command_handler_settings_get;

// Обработчик команды установки настроек даты/времени
static class ntime_command_handler_settings_set_t : public ipc_responder_template_t<time_command_settings_set_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        // Установка настроек
        ntime_sync_settings = command.request;
        storage_modified();
        // Передача подтверждения
        transmit();
        // Передача списка хостов
        ntime_command_handler_hostlist.upload();
        // Коррекция даты синхронизации
        if (!ntime_sync_settings.can_sync())
            ntime_sync_time_clear();
    }
} ntime_command_handler_settings_set;

void ntime_init(void)
{
    // Обработчики IPC
    esp_handler_add(ntime_command_handler_hostlist);
    esp_handler_add(ntime_command_handler_time_sync);
    esp_handler_add(ntime_command_handler_current_get);
    esp_handler_add(ntime_command_handler_current_set);
    esp_handler_add(ntime_command_handler_settings_get);
    esp_handler_add(ntime_command_handler_settings_set);
    esp_handler_add(ntime_command_handler_time_sync_start);
    // Запуск синхронизации
    ntime_sync();
}

void ntime_sync(void)
{
    ntime_sync_time_clear();
    ntime_command_handler_time_sync.go();
}
