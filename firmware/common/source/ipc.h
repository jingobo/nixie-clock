// IPC - Internal Packet Connection
#ifndef __IPC_H
#define __IPC_H

#include "list.h"

// Размер полей канального слоя
constexpr const size_t IPC_DLL_SIZE = 4;
// Максимальный размер прикладного слоя
constexpr const size_t IPC_APL_SIZE = 28;
// Общий размер пакета
constexpr const size_t IPC_PKT_SIZE = IPC_DLL_SIZE + IPC_APL_SIZE;

// Количество слотов пакетов в одну сторону
constexpr const auto IPC_SLOT_COUNT = 10;

// TODO: конечная корректировка лимитов
// Поддерживаемые коды команды
enum ipc_opcode_t : uint8_t
{
    // Команда управления потоком
    IPC_OPCODE_FLOW = 0,
    
    // Не команда, база для команд, обрабатываемых ядром STM32
    IPC_OPCODE_STM_HANDLE_BASE = 1,
        // Запрос текущей даты/времени
        IPC_OPCODE_STM_TIME_GET,
        // Установка текущей даты/времени
        IPC_OPCODE_STM_TIME_SET,
        // Синхронизация даты/времени
        IPC_OPCODE_STM_TIME_SYNC_START,
        
        // Запрос настроек даты/времени
        IPC_OPCODE_STM_TIME_SETTINGS_GET,
        // Установка настроек даты/времени
        IPC_OPCODE_STM_TIME_SETTINGS_SET,
        
        // Отчет о присвоении IP 
        IPC_OPCODE_STM_WIFI_IP_REPORT,
        // Запрос настроек WiFi
        IPC_OPCODE_STM_WIFI_SETTINGS_GET,
        // Установка настроек WiFi
        IPC_OPCODE_STM_WIFI_SETTINGS_SET,

        // Получает состояние экрана
        IPC_OPCODE_STM_SCREEN_STATE_GET,
        
        // Запрос настроек сцены времени
        IPC_OPCODE_STM_DISPLAY_TIME_GET,
        // Установка настроек сцены времени
        IPC_OPCODE_STM_DISPLAY_TIME_SET,

        // Получает состояние освещенности
        IPC_OPCODE_STM_LIGHT_STATE_GET,
        // Получает настройки освещенности
        IPC_OPCODE_STM_LIGHT_SETTINGS_GET,
        // Задает настройки освещенности
        IPC_OPCODE_STM_LIGHT_SETTINGS_SET,
        
    // Не команда, база для команд, обрабатываемых модулем ESP8266
    IPC_OPCODE_ESP_HANDLE_BASE = 25,
        // Запрос информации о сети
        IPC_OPCODE_ESP_WIFI_INFO_GET,
        // Поиск сетей с опросом состояния
        IPC_OPCODE_ESP_WIFI_SEARCH_POOL,
        // Оповещение, что настройки WiFi сменились
        IPC_OPCODE_ESP_WIFI_SETTINGS_CHANGED,
        
        // Запрос даты/времени из интернета
        IPC_OPCODE_ESP_TIME_SYNC,
        // Передача списка хостов SNTP
        IPC_OPCODE_ESP_TIME_HOSTLIST_SET,

    // Не команда, определяет лимит количества команд
    IPC_OPCODE_LIMIT = 50,
};

// Тип направления
enum ipc_dir_t : bool
{
    // Запрос
    IPC_DIR_REQUEST = 0,
    // Ответ
    IPC_DIR_RESPONSE
};

// Структура пакета [32 байта]
struct ipc_packet_t
{
    // Канальный слой
    struct
    {
        // Контрольная сумма
        uint16_t checksum;
        // Параметры передачи
        struct
        {
            // Код команды
            ipc_opcode_t opcode;
            // Длинна текущих данных
            uint8_t length : 5;
            // Указывает направление (запрос/ответ)
            ipc_dir_t dir : 1;
            // Фаза передачи (для контроля пропущенного пакета)
            bool phase : 1;
            // Есть ли еще данные для этой команды
            bool more : 1;
        };
    } dll;
    
    // Прикладной слой
    uint8_t apl[IPC_APL_SIZE];
    
    // Подсчет контрольной суммы
    uint16_t checksum_get(void) const;
    
    // Получает признак последнего пакета
    bool last_get(void) const
    {
        return !dll.more;
    }
    
    // Начальная подготовка пакета перед заполнением
    void prepare(ipc_opcode_t opcode, ipc_dir_t dir)
    {
        dll.dir = dir;
        dll.opcode = opcode;
    }
    
    // Проверяет на равенство кода кодманды и направления
    bool equals(ipc_opcode_t opcode, ipc_dir_t dir) const
    {
        return dll.opcode == opcode && dll.dir == dir;
    }
    
    // Проверяет на эквивалентность пакетов (код кодманды и направления одинаков)
    bool equals(const ipc_packet_t &other) const
    {
        return equals(other.dll.opcode, other.dll.dir);
    }
};

// Проверка размера пакета
STATIC_ASSERT(sizeof(ipc_packet_t) == IPC_PKT_SIZE);

// Класс списка пакетов
class ipc_slots_t
{
    // Доступные слоты пакетов
    struct ipc_slot_t : list_item_t
    {
        // Исходный пакет
        ipc_packet_t packet;
    } slots[IPC_SLOT_COUNT];
    // Фаза передачи
    bool phase = false;
public:
    // Списки свободных и используемых слотов
    list_template_t<ipc_slot_t> unused, used;

    // Конструктор по умолчанию
    ipc_slots_t(void);
    
    // Перенос слота в используемые
    void use(ipc_slot_t &slot);
    // Перенос слота в свободные
    void free(ipc_slot_t &slot);
    // Отчистка (перевод всех слотов в список свободных)
    void clear(void);
    
    // Возвращает, пуста ли очередь используемых пакетов
    bool empty(void) const
    {
        return used.empty();
    }

    // Смена фазы передачи, возвращает старое значение
    bool phase_switch(void)
    {
        auto result = phase;
        phase = !phase;
        return result;
    }
};

// Класс интерфейс процессора пакетов
class ipc_processor_t
{
public:
    // Аргументы обработки данных данных
    struct args_t
    {
        // Указывает, первый ли пакет
        bool first = true;
        // Размер полученных данных
        const size_t size;

        // Конструктор по умолчанию
        args_t(size_t _size) : size(_size)
        { }
    };

    // Обработка пакета
    virtual bool packet_process(const ipc_packet_t &packet, const args_t &args) = 0;
    // Разбитие данных на пакеты
    static bool data_split(ipc_processor_t &processor, ipc_opcode_t opcode, ipc_dir_t dir, const void *source, size_t size);
};

// Класс прокси процессора пакетов
class ipc_processor_proxy_t : public ipc_processor_t
{
    // Прототип функции обработчика пакета
    typedef bool (* packet_process_proc_ptr)(const ipc_packet_t &packet, const args_t &args);

    // Функция обработчика пакета
    const packet_process_proc_ptr handler;
public:
    // Конструктор по умолчанию
    ipc_processor_proxy_t(packet_process_proc_ptr _handler) : handler(_handler)
    {
        assert(_handler != NULL);
    }

    // Обработка пакета
    virtual bool packet_process(const ipc_packet_t &packet, const args_t &args) override final
    {
        return handler(packet, args);
    }
};

// Базовый контроллер пакетов
class ipc_link_t : public ipc_processor_t
{
protected:
    // Причина сброса
    enum reset_reason_t : uint8_t
    {
        // Нет действий
        RESET_REASON_NOP = 0,
        // Переполнение слотов
        RESET_REASON_OVERFLOW,
        // Искажение данных
        RESET_REASON_CORRUPTION,
        
        // Для определения количества значений
        RESET_REASON_COUNT
    };
    
    // Признак пропуска пакетов
    bool skip;
    // Слоты на приём/передачу
    ipc_slots_t tx, rx;
    
    // Обработка входящих пакетов
    void flush_packets(ipc_processor_t &receiver);
    // Проверка фазы полученного пакета при приёме
    virtual bool check_phase(const ipc_packet_t &packet);
    // Сброс прикладного уровня
    virtual void reset_layer(reset_reason_t reason, bool internal = true);
private:
    // Флаг, указывающий, что происходит сброс инициированый нами
    bool reseting = false;
    
    // Передача команды управления потоком
    void transmit_flow(reset_reason_t reason);
public:
    // Сброс слотов, полей
    void reset(void);
    // Получение пакета к выводу
    virtual void packet_output(ipc_packet_t &packet);
    // Ввод полученного пакета
    virtual bool packet_input(const ipc_packet_t &packet);
    // Обработка пакета (перенос в передачу)
    virtual bool packet_process(const ipc_packet_t &packet, const args_t &args) override;
};

// Базовый класс команды
class ipc_command_t
{
    friend class ipc_handler_host_t;
    
    // Код команды
    const ipc_opcode_t opcode;
protected:
    // Конструктор по умолчанию
    ipc_command_t(ipc_opcode_t _opcode) : opcode(_opcode)
    { }

    // Получает размер буфера
    virtual size_t buffer_size(ipc_dir_t dir) const;
    // Получает указатель буфера
    virtual const void * buffer_pointer(ipc_dir_t dir) const;
    
    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir);
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size);
public:
    // Передача команды
    bool transmit(ipc_processor_t &processor, ipc_dir_t dir);
};

// Шаблон команды с фиксированным размером запроса/ответа
template <typename REQUEST, typename RESPONSE>
class ipc_command_fixed_t : public ipc_command_t
{
public:
    // Поля запроса
    REQUEST request;
    // Поля ответа
    RESPONSE response;
protected:
    // Получает размер буфера
    virtual size_t buffer_size(ipc_dir_t dir) const override final
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return sizeof(request);
                
            case IPC_DIR_RESPONSE:
                return sizeof(response);
                
            default:
                return ipc_command_t::buffer_size(dir);
        }
    }
    
    // Получает указатель буфера
    virtual const void * buffer_pointer(ipc_dir_t dir) const override final
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return &request;
                
            case IPC_DIR_RESPONSE:
                return &response;
                
            default:
                return ipc_command_t::buffer_pointer(dir);
        }
    }

    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir) override final
    {
        // Проверка данных
        assert(dir != IPC_DIR_RESPONSE || response.check());
        assert(dir != IPC_DIR_REQUEST || request.check());
        
        // Возвращаем размер из базового метода
        return ipc_command_t::encode(dir);
    }
    
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size) override final
    {
        // Проверка данных
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                if (!request.check())
                    return false;
                break;
                
            case IPC_DIR_RESPONSE:
                if (!response.check())
                    return false;
                break;
        }
        
        // Возвращаем результат из базового метода
        return ipc_command_t::decode(dir, size);
    }

    // Конструктор по умолчанию
    ipc_command_fixed_t(ipc_opcode_t opcode) : ipc_command_t(opcode)
    { }
};

// Шаблон команды запроса данных (пустой запрос с фиксированным размером ответа)
template <typename RESPONSE>
class ipc_command_get_t : public ipc_command_t
{
public:
    // Поля ответа
    RESPONSE response;
protected:
    // Получает размер буфера
    virtual size_t buffer_size(ipc_dir_t dir) const override final
    {
        switch (dir)
        {
            case IPC_DIR_RESPONSE:
                return sizeof(response);
                
            default:
                return ipc_command_t::buffer_size(dir);
        }
    }
    
    // Получает указатель буфера
    virtual const void * buffer_pointer(ipc_dir_t dir) const override final
    {
        switch (dir)
        {
            case IPC_DIR_RESPONSE:
                return &response;
                
            default:
                return ipc_command_t::buffer_pointer(dir);
        }
    }

    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir) override final
    {
        // Проверка данных
        assert(dir != IPC_DIR_RESPONSE || response.check());
        
        // Возвращаем размер из базового метода
        return ipc_command_t::encode(dir);
    }
    
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size) override final
    {
        // Проверка данных
        switch (dir)
        {
            case IPC_DIR_RESPONSE:
                if (!response.check())
                    return false;
                
            default:
                // Возвращаем результат из базового метода
                return ipc_command_t::decode(dir, size);
        }
    }

    // Конструктор по умолчанию
    ipc_command_get_t(ipc_opcode_t opcode) : ipc_command_t(opcode)
    { }
};

// Шаблон команды установки данных (пустой ответ с фиксированным запросом)
template <typename REQUEST>
class ipc_command_set_t : public ipc_command_t
{
public:
    // Поля запроса
    REQUEST request;
protected:
    // Получает размер буфера
    virtual size_t buffer_size(ipc_dir_t dir) const override final
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return sizeof(request);
                
            default:
                return ipc_command_t::buffer_size(dir);
        }
    }
    
    // Получает указатель буфера
    virtual const void * buffer_pointer(ipc_dir_t dir) const override final
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return &request;
                
            default:
                return ipc_command_t::buffer_pointer(dir);
        }
    }

    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir) override final
    {
        // Проверка данных
        assert(dir != IPC_DIR_REQUEST || request.check());
        
        // Возвращаем размер из базового метода
        return ipc_command_t::encode(dir);
    }
    
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size) override final
    {
        // Проверка данных
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                if (!request.check())
                    return false;
                
            default:
                // Возвращаем результат из базового метода
                return ipc_command_t::decode(dir, size);
        }
    }

    // Конструктор по умолчанию
    ipc_command_set_t(ipc_opcode_t opcode) : ipc_command_t(opcode)
    { }
};

// Класс хоста обработчиков команд (предварительное объявление)
class ipc_handler_host_t;

// Базовый класс обработчика команды
class ipc_handler_t : list_item_t
{
    friend class ipc_handler_host_t;
protected:
    // Тип для хранения тиков в мС
    typedef uint32_t tick_t;
    
    // Локальные константы
    enum
    {
        // Таймут в тика мС для переотправки
        TIMEOUT_RETRY = 50,
    };
private:
    // Время последней передачи
    tick_t transmit_time;
protected:
    // Получает текущее значение тиков (реализуется платформой)
    static tick_t tick_get(void);
    // Получает процессор для передачи (реализуется платформой)
    static ipc_processor_t & transmitter_get(void);
    
    // Класс счетчика времени
    class timer_t
    {
        // Начало отсчета, задержка
        tick_t time, delay;
        // Активность
        bool enabled = false;
    public:
        // Остановка таймера
        void stop(void)
        {
            enabled = false;
        }
        
        // Старт таймера
        void start(tick_t ticks = 0)
        {
            time = tick_get();
            enabled = true;
            delay = ticks;
        }
        
        // Истек ли таймер
        bool elapsed(bool autostop = true)
        {
            if (!enabled || tick_get() - time < delay)
                return false;
            
            if (autostop)
                stop();
            return true;
        }
    };

    // Получает количество тиков на момент последней передачи
    tick_t transmit_time_get(void) const
    {
        return transmit_time;
    }

    // Оповещение о обработке
    virtual void pool(void) = 0;
    // Событие обработки данных
    virtual void work(bool idle) = 0;
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir) = 0;
    // Получает ссылку на команду
    virtual ipc_command_t & command_get(void) = 0;
    
    // Передача команды (внутренний метод)
    bool transmit_internal(ipc_dir_t dir)
    {
        transmit_time = tick_get();
        return command_get().transmit(transmitter_get(), dir);
    }
};

// Базовый класс обработчика команды (запроситель)
class ipc_requester_t : public ipc_handler_t
{
    // Таймут на перезапрос
    const tick_t timeout_request;

    // Перечисление состяония обработчика
    enum handler_state_t
    {
        // Простой
        HANDLER_STATE_IDLE,
        // Передача запроса
        HANDLER_STATE_REQUEST,
        // Ожидание ответа
        HANDLER_STATE_RESPONSE_WAIT,
        // Ожидание обрабоки ответа
        HANDLER_STATE_RESPONSE_PENDING,
    } state = HANDLER_STATE_IDLE;
protected:
    // Локальные константы
    enum
    {
        // Таймут в тика мС для перезапроса по умолчанию
        TIMEOUT_REPEAT_DEFAULT = 450,
    };

    // Передача
    void transmit(void);
    // Оповещение о обработке
    virtual void pool(void) override final;

    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir) override final
    {
        assert(dir == IPC_DIR_RESPONSE);
        
        // Переход к ожиданию обработки только если ожидали ответ
        if (state == HANDLER_STATE_RESPONSE_WAIT)
            state = HANDLER_STATE_RESPONSE_PENDING;
    }

    // Конструктор по умолчанию
    ipc_requester_t(tick_t timeout_request = TIMEOUT_REPEAT_DEFAULT) : 
        timeout_request(timeout_request)
    { }
};

// Шаблон класса обработчика команды (ответчик)
template <typename COMMAND>
class ipc_requester_template_t : public ipc_requester_t
{
public:
    // Комадна
    COMMAND command;
protected:
    // Получает ссылку на команду
    virtual ipc_command_t &command_get(void) override final
    {
        return command;
    }
    
    // Конструктор по умолчанию
    ipc_requester_template_t(tick_t timeout_request = TIMEOUT_REPEAT_DEFAULT) :
        ipc_requester_t(timeout_request)
    { }
};

// Базовый класс обработчика команды (ответчик)
class ipc_responder_t : public ipc_handler_t
{
    // Перечисление состяония обработчика
    enum handler_state_t
    {
        // Простой
        HANDLER_STATE_IDLE,
        // Ожидание обработки запроса
        HANDLER_STATE_REQUEST_PENDING,
        // Передача ответа
        HANDLER_STATE_RESPONSE,
    } state = HANDLER_STATE_IDLE;
protected:
    // Передача
    void transmit(void);
    // Оповещение о обработке
    virtual void pool(void) override final;

    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir) override final
    {
        assert(dir == IPC_DIR_REQUEST);
        
        // Переход к ожиданию обработки только если были в простое
        if (state == HANDLER_STATE_IDLE)
            state = HANDLER_STATE_REQUEST_PENDING;
    }
};

// Шаблон класса обработчика команды (запроситель)
template <typename COMMAND>
class ipc_responder_template_t : public ipc_responder_t
{
public:
    // Комадна
    COMMAND command;
protected:
    // Получает ссылку на команду
    virtual ipc_command_t &command_get(void) override final
    {
        return command;
    }
};

// Класс хоста обработчиков команд
class ipc_handler_host_t : public ipc_processor_t
{
    // Данные связанные с текущей сборкой
    struct
    {
        // Смещение записи в байтах
        size_t offset;
        // Используемый обработчик
        ipc_handler_t *handler;
    } processing;
    // Список обработчиков
    list_template_t<ipc_handler_t> handlers;
    
    // Поиск обработчика по команде
    ipc_handler_t * handler_find(ipc_opcode_t opcode) const;
public:
    // Оповещение о обработке
    void pool(void);
    // Добавление обработчика в хост
    void handler_add(ipc_handler_t &handler);
    // Обработка пакета (склеивание в команду)
    virtual bool packet_process(const ipc_packet_t &packet, const args_t &args) override final;
};

// Валидация булевы на этапе компиляции
STATIC_ASSERT(true == 1);
STATIC_ASSERT(sizeof(bool) == 1);

// Валидация булевы на этапе выполнения
inline bool ipc_bool_check(bool value)
{
    // Промежуточная структура
    const union
    {
        // Как булева
        bool b;
        // Как число
        uint8_t u8;
    } u = { value };
    
    return u.u8 < 2;
}

// Возвращает количество символов
int16_t ipc_string_length(const char *s, size_t size);

#endif // __IPC_H
