// IPC - Internal Packet Connection
#ifndef __IPC_H
#define __IPC_H

#include "list.h"

// Размер полей канального слоя
#define IPC_DLL_SIZE            4
// Максимальный размер прикладного слоя
#define IPC_APL_SIZE            28
// Общий размер пакета
#define IPC_PKT_SIZE            (IPC_DLL_SIZE + IPC_APL_SIZE)

// Количество слотов пакетов в одну сторону
#define IPC_SLOT_COUNT          10

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
        
        // Запрос настроек даты/времени
        IPC_OPCODE_STM_TIME_SETTINGS_GET,
        // Установка настроек даты/времени
        IPC_OPCODE_STM_TIME_SETTINGS_SET,
        
        // Запрос настроек WiFi
        IPC_OPCODE_STM_WIFI_SETTINGS_GET,
        // Установка настроек WiFi
        IPC_OPCODE_STM_WIFI_SETTINGS_SET,

    // Не команда, база для команд, обрабатываемых модулем ESP8266
    IPC_OPCODE_ESP_HANDLE_BASE = 50,
        // Оповещение, что настройки WiFi сменились
        IPC_OPCODE_ESP_WIFI_SETTINGS_CHANGED,
        
        // Запрос даты/времени из интернета
        IPC_OPCODE_ESP_TIME_SYNC,
        // Передача списка хостов SNTP
        IPC_OPCODE_ESP_TIME_HOSTLIST_SET,
    // Не команда, определяет лимит количества команд
    IPC_OPCODE_LIMIT = 100,
};

// Тип направления
enum ipc_dir_t : uint8_t
{
    // Запрос
    IPC_DIR_REQUEST = 0,
    // Ответ
    IPC_DIR_RESPONSE
};

// Тип для булевы
enum ipc_bool_t : uint8_t
{
    // Ложь
    IPC_BOOL_FALSE = 0,
    // Истина
    IPC_BOOL_TRUE
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
            // Указывает направление (запрос/ответ)
            ipc_dir_t dir : 1;
            
            // Есть ли еще данные для этой команды
            ipc_bool_t more : 1;
            // Длинна текущих данных
            uint8_t length : 5;
            
            // Фаза передачи (для контроля пропущенного пакета)
            ipc_bool_t phase : 1;
        };
    } dll;
    
    // Прикладной слой
    union
    {
        uint8_t u8[IPC_APL_SIZE / sizeof(uint8_t)];
        uint16_t u16[IPC_APL_SIZE / sizeof(uint16_t)];
        uint32_t u32[IPC_APL_SIZE / sizeof(uint32_t)];
    } apl;
    
    // Подсчет контрольной суммы
    uint16_t checksum_get(void) const;
    // Начальная подготовка пакета перед заполнением
    void prepare(ipc_opcode_t opcode, ipc_dir_t dir);
    
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
static_assert(sizeof(ipc_packet_t) == IPC_PKT_SIZE);

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
    ipc_bool_t phase = IPC_BOOL_FALSE;
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
    ipc_bool_t phase_switch(void)
    {
        auto result = phase;
        phase = phase ? IPC_BOOL_FALSE : IPC_BOOL_TRUE;
        return result;
    }
};

// Класс интерфейс процессора пакетов
class ipc_processor_t
{
protected:
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
public:
    // Обработка пакета
    virtual bool packet_process(const ipc_packet_t &packet, const args_t &args) = 0;
    // Разбитие данных на пакеты
    static bool data_split(ipc_processor_t &processor, ipc_opcode_t opcode, ipc_dir_t dir, const void *source, size_t size);
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
    // Слоты на приём/передачу
    ipc_slots_t tx, rx;
    
    // Проверка фазы полученного пакета при приёме
    virtual bool check_phase(const ipc_packet_t &packet);
    // Сброс прикладного уровня
    virtual void reset_layer(reset_reason_t reason, bool internal = true);
    // Обработка входящих пакетов
    void process_incoming_packets(ipc_processor_t &receiver);
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
    virtual void packet_input(const ipc_packet_t &packet);
    // Обработка пакета (перенос в передачу)
    virtual bool packet_process(const ipc_packet_t &packet, const args_t &args);
};

// Контроллер пакетов (слейв)
typedef ipc_link_t ipc_link_slave_t;

// Контроллер пакетов (мастер)
class ipc_link_master_t : public ipc_link_t
{
    // Контроль переотправки исходящих данных
    struct retry_t
    {
        // Два кэшированных пакета
        ipc_packet_t packet[2];
        // Индекс передаваемого пакета
        uint8_t index;

        // Конструктор по умолчанию
        retry_t(void) : index(ARRAY_SIZE(packet))
        { }
    } retry;
    // Счетчик ошибок передачи
    uint8_t corruption_count = 0;
protected:
    // Событие массового сброса (другая сторона не отвечает)
    virtual void reset_slave(void) = 0;
    
    // Проверка фазы полученного пакета
    virtual bool check_phase(const ipc_packet_t &packet);
    // Сброс прикладного уровня
    virtual void reset_layer(reset_reason_t reason, bool internal);
public:
    // Получение пакета к выводу
    virtual void packet_output(ipc_packet_t &packet);
    // Ввод полученного пакета
    virtual void packet_input(const ipc_packet_t &packet);
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
    virtual size_t buffer_size(ipc_dir_t dir) const
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
    virtual const void * buffer_pointer(ipc_dir_t dir) const
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
    virtual size_t encode(ipc_dir_t dir)
    {
        // Проверка данных
        assert(dir != IPC_DIR_RESPONSE || response.check());
        assert(dir != IPC_DIR_REQUEST || request.check());
        // Возвращаем размер из базового метода
        return ipc_command_t::encode(dir);
    }
    
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size)
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
    virtual size_t buffer_size(ipc_dir_t dir) const
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
    virtual const void * buffer_pointer(ipc_dir_t dir) const
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
    virtual size_t encode(ipc_dir_t dir)
    {
        // Проверка данных
        assert(dir != IPC_DIR_RESPONSE || response.check());
        // Возвращаем размер из базового метода
        return ipc_command_t::encode(dir);
    }
    
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size)
    {
        // Проверка данных
        switch (dir)
        {
            case IPC_DIR_RESPONSE:
                if (!response.check())
                    return false;
                break;
        }
        // Возвращаем результат из базового метода
        return ipc_command_t::decode(dir, size);
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
    virtual size_t buffer_size(ipc_dir_t dir) const
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
    virtual const void * buffer_pointer(ipc_dir_t dir) const
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
    virtual size_t encode(ipc_dir_t dir)
    {
        // Проверка данных
        assert(dir != IPC_DIR_REQUEST || request.check());
        // Возвращаем размер из базового метода
        return ipc_command_t::encode(dir);
    }
    
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size)
    {
        // Проверка данных
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                if (!request.check())
                    return false;
                break;
        }
        // Возвращаем результат из базового метода
        return ipc_command_t::decode(dir, size);
    }

    // Конструктор по умолчанию
    ipc_command_set_t(ipc_opcode_t opcode) : ipc_command_t(opcode)
    { }
};

// Базовый класс обработчика команды
class ipc_handler_t : list_item_t
{
    friend class ipc_handler_host_t;
protected:
    // Тип для хранения тиков в мС
    typedef uint32_t tick_t;
    
    // Получает текущее значение тиков (реализуется платформой)
    static tick_t tick_get(void);
    // Получает процессор для передачи (реализуется платформой)
    static ipc_processor_t & processor_get(void);
    
    // Получает ссылку на команду
    virtual ipc_command_t & command_get(void) = 0;
private:
    // Внутреннее состяоние обработчика
    enum
    {
        // Простой
        INTERNAL_STATE_IDLE,
        // Запрос
        INTERNAL_STATE_REQUEST,
        // Обработка запроса
        INTERNAL_STATE_REQUEST_WAIT,
        // Ответ
        INTERNAL_STATE_RESPONSE,
        // Ожидание ответа
        INTERNAL_STATE_RESPONSE_WAIT,
    } istate = INTERNAL_STATE_IDLE;
    
    // Время последней передачи
    tick_t transmit_time;

    // Таймуты
    const struct timeouts_t
    {
        // Переотправка
        tick_t retry;
        // Обработка запроса
        tick_t request;
        
        // Конструктор по умолчанию
        timeouts_t(tick_t _retry, tick_t _request) :
            retry(_retry), request(_request)
        { }
    } timeout;
    
    // Передача команды (внутренний метод)
    bool transmit_internal(ipc_dir_t dir)
    {
        transmit_time = tick_get();
        return command_get().transmit(processor_get(), dir);
    }
protected:
    // Общее состояние обработчика
    enum state_t
    {
        // Простой
        STATE_IDLE,
        // Ожидание обработки запроса
        STATE_REQUEST_PENDING,
        // Передача запроса/ответа
        STATE_TRANSMIT_COMMAND,
    };

    // Получает общее состояние обработчика
    state_t state_get(void) const
    {
        switch (istate)
        {
            case INTERNAL_STATE_IDLE:
                return STATE_IDLE;
            case INTERNAL_STATE_REQUEST_WAIT:
                return STATE_REQUEST_PENDING;
            default:
                return STATE_TRANSMIT_COMMAND;
        }
    }
    
    // Оповещение о поступлении данных
    void notify(ipc_dir_t dir)
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                istate = INTERNAL_STATE_REQUEST_WAIT;
                break;
            case IPC_DIR_RESPONSE:
                istate = INTERNAL_STATE_IDLE;
                break;
        }
    }
    
    // Оповещение о обработке
    virtual void pool(void)
    {
        switch (istate)
        {
            case INTERNAL_STATE_IDLE:
            case INTERNAL_STATE_REQUEST_WAIT:
                // Ничего не делаем
                return;
            case INTERNAL_STATE_REQUEST:
            case INTERNAL_STATE_RESPONSE:
                // Таймаут на переотправку
                if (tick_get() - transmit_time > timeout.retry)
                    transmit((istate != INTERNAL_STATE_RESPONSE) ?
                        IPC_DIR_REQUEST :
                        IPC_DIR_RESPONSE);
                return;
            case INTERNAL_STATE_RESPONSE_WAIT:
                // Таймаут на перезапрос
                if (tick_get() - transmit_time > timeout.request)
                    transmit(IPC_DIR_REQUEST);
                return;
            default:
                assert(false);
        }
    }

    // Конструктор по умолчанию
    ipc_handler_t(tick_t timeout_retry, tick_t timeout_request) :
        timeout(timeout_retry, timeout_request)
    { }
public:
    // Передача команды
    void transmit(ipc_dir_t dir)
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                if (transmit_internal(dir))
                    // Запрос передан - ожидание ответа
                    istate = INTERNAL_STATE_RESPONSE_WAIT;
                else
                    // Запрос не передан - переотправка запроса
                    istate = INTERNAL_STATE_REQUEST;
                return;
            case IPC_DIR_RESPONSE:
                if (transmit_internal(dir))
                    // Ответ передан - переход к простою
                    istate = INTERNAL_STATE_IDLE;
                else
                    // Ответ передан - переотправка ответа
                    istate = INTERNAL_STATE_RESPONSE;
                return;
        }
    }
};

// Шаблон класса обработчика команды
template <typename COMMAND>
class ipc_handler_template_t : public ipc_handler_t
{
public:
    // Комадна
    COMMAND command;
protected:
    // Получает ссылку на команду
    virtual ipc_command_t &command_get(void)
    {
        return command;
    }
};

// Класс хоста обработчиков команд
class ipc_handler_host_t : public ipc_processor_t
{
private:
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
    ipc_handler_t * find_handler(ipc_opcode_t opcode) const;
public:
    // Оповещение о обработке
    void pool(void);
    // Добавление обработчика в хост
    void add_handler(ipc_handler_t &handler);
    
    // Обработка пакета (склеивание в команду)
    virtual bool packet_process(const ipc_packet_t &packet, const args_t &args);
};

// Возвращает количество символов
int16_t ipc_string_length(const char *s, size_t size);

#endif // __IPC_H
