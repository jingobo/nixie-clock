#include "rtc.h"
#include "esp.h"
#include "wifi.h"
#include "ntime.h"
#include "random.h"
#include "storage.h"
#include <proto/time.inc.h>

// Настройки синхронизации
static time_sync_settings_t ntime_sync_settings @ STORAGE_SECTION =
{
    // Синхронизация включена
    .sync = true,
    // UTC +03:00
    .timezone = 6,
    // Стандартное время
    .offset = 0,
    // Список хостов по умолчанию
    .hosts = "ntp1.stratum2.ru\n0.pool.ntp.org"
};

// Время последней синхронизации
static __no_init datetime_t ntime_sync_time;
// Количество секунд времени работы с последней синхронизации
static __no_init uint32_t ntime_sync_seconds;

// Очистка даты последней синхронизации
static void ntime_sync_time_clear(void)
{
    memory_clear(&ntime_sync_time, sizeof(ntime_sync_time));
}

// Сброс количества секунд с последней синхронизации
static void ntime_sync_seconds_reset(void)
{
    ntime_sync_seconds = 0;
}

// Получает, можно ли запустить процедуру синхронизации
static bool ntime_sync_allow(void)
{
    // Есть ли гепотетически интернет и можем ли синхронизировать
    return wifi_has_internet_get() && ntime_sync_settings.sync_allow();
}

// Обработчик команды чтения даты/времени
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
        // Проверяем возможность синхронизации
        if (ntime_sync_allow())
        {
            request_timer.start();
            try_count = 0;
            return true;
        }
        
        request_timer.stop();
        return false;
    }
} ntime_command_handler_time_sync;

// Класс автосинхронизации даты/времени
static class ntime_synchronizer_t
{
    // Признак обработки
    bool running = false;
    // Результат обработки
    bool success = false;
    // Секундный таймаут
    uint32_t remain_timeout;
    
    // Таймаут опроса
    void timeout_pool(void)
    {
        // Каждые 5 минут
        remain_timeout = datetime_t::SECONDS_PER_MINUTE * 5;
    }
public:
    // Конструктор по умолчанию
    ntime_synchronizer_t(void)
    {
        timeout_pool();
    }

    // Запуск обработки
    void run(void)
    {
        // Выход если обработка
        if (running)
            return;
        
        // Запуск синхронизации
        ntime_sync_time_clear();
        if (ntime_command_handler_time_sync.go())
            running = true;
        else
            timeout_pool();
    }
    
    // Получает признак обработки
    bool running_get(void) const
    {
        return running;
    }
    
    // Получает результат обработки
    bool success_get(void) const
    {
        return success;
    }

    // Обработчик секундного события
    void second(void)
    {
        // Выход если обработка
        if (running)
            return;
        
        // Обработка секундного таймаута
        if (remain_timeout > 0)
            remain_timeout--;
        else
            run();
    }
    
    // Репортирование о результате обработки
    void report(bool status)
    {
        // Обновление состояний
        running = false;
        success = status;
        
        // Подбор следующего таймаута
        if (success)
            // Раз в 5-8 дней
            remain_timeout = datetime_t::SECONDS_PER_DAY * 5 +  random_range_get(0, datetime_t::SECONDS_PER_DAY * 3);
        else if (rtc_uptime_seconds > datetime_t::SECONDS_PER_HOUR)
            // Раз 1-2 часа
            remain_timeout = datetime_t::SECONDS_PER_HOUR + random_range_get(0, datetime_t::SECONDS_PER_HOUR);
        else
            // Часто в первый час
            timeout_pool();
    }
} ntime_synchronizer;

// Обработчик команды запуска синхронизации даты/времени
static class ntime_command_handler_time_sync_start_t : public ipc_responder_template_t<time_command_sync_start_t>
{
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
                ntime_synchronizer.run();
                break;
                
            case time_command_sync_start_request_t::ACTION_CHECK:
                // Ничего не делаем
                break;
        }
        
        // Передача результата
        if (ntime_synchronizer.running_get())
            command.response.status = time_command_sync_start_response_t::STATUS_PENDING;
        else
            command.response.status = ntime_synchronizer.success_get() ?
                time_command_sync_start_response_t::STATUS_SUCCESS :
                time_command_sync_start_response_t::STATUS_FAILED;
        
        transmit();
    }
} ntime_command_handler_time_sync_start;

// Обработчик команды чтения списка SNTP хостов
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

// Применение времени синхронизации
static void ntime_sync_apply(const datetime_t &fresh)
{
    // Коррекция частоты если есть секунды с последней синхронизации
    if (ntime_sync_seconds > 0)
    {
        // Секунд по часам устройства
        const auto device_seconds = rtc_uptime_seconds - ntime_sync_seconds;
        
        // Если дельта секунд достаточная
        if (device_seconds > RTC_LSE_FREQ_DEFAULT)
        {
            auto seconds = device_seconds;
            seconds =+ rtc_time.utc_to_seconds() - fresh.utc_to_seconds();
            
            seconds *= rtc_lse_freq;
            seconds /= device_seconds;
            rtc_lse_freq_set((uint16_t)seconds);
        }
    }
    
    // Приминение времени
    ntime_sync_seconds = rtc_uptime_seconds;
    rtc_time_set(fresh);
}

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
            if (ntime_sync_settings.sync_allow())
                ntime_sync_apply(command.response.value);
            
            // Рапортирование автомату синхронизации
            ntime_synchronizer.report(true);
            break;
            
        case time_command_sync_response_t::STATUS_FAILED:
            // Если есть еще попытки и есть WiFi
            if (try_count > 10 || !wifi_has_internet_get())
            {
                // Рапортирование автомату синхронизации
                ntime_synchronizer.report(false);
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
                ntime_synchronizer.report(false);
            break;
            
        default:
            assert(false);
            break;
    }
}

// Обработчик команды чтения текущей даты/времени
static class ntime_command_handler_current_get_t : public ipc_responder_template_t<time_command_current_get_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        
        // Заполняем ответ
        command.response.time.sync = ntime_sync_time;
        command.response.time.current = rtc_time;
        command.response.time.lse = rtc_lse_freq;
        command.response.time.uptime = rtc_uptime_seconds;
        command.response.sync_allow = ntime_sync_allow();
        
        // Передача ответа
        transmit();
    }
} ntime_command_handler_current_get;

// Обработчик команды записи текущей даты/времени
static class ntime_command_handler_current_set_t : public ipc_responder_template_t<time_command_current_set_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;
        
        // Установка даты/времени
        rtc_time = command.request;
        // Сброс данных синхронизации
        ntime_sync_time_clear();
        ntime_sync_seconds_reset();
        // Передача подтверждения
        transmit();
    }
} ntime_command_handler_current_set;

// Обработчик команды чтения настроек даты/времени
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

// Обработчик команды записи настроек даты/времени
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
    }
} ntime_command_handler_settings_set;

// Событие наступления секунды
static list_handler_item_t ntime_second_event([](void)
{
    ntime_synchronizer.second();
});

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
    
    // Подготовка синхронизации
    ntime_synchronizer.run();
    ntime_sync_seconds_reset();
    rtc_second_event_add(ntime_second_event);
}
