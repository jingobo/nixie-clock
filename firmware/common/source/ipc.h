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
// Проверка размера пакета
#define IPC_PKT_SIZE_CHECK()     assert(sizeof(ipc_packet_t) == IPC_PKT_SIZE)

// Количество слотов пакетов в одну сторону
#define IPC_SLOT_COUNT          10

// Поддерживаемые команды
enum ipc_command_t
{
    IPC_COMMAND_FLOW = 0,
    
    // Не команда, база для команд, обрабатываемых ядром STM32
    IPC_COMMAND_STM_HANDLE_BASE = 1,
    // Синхронизация времени
    IPC_COMMAND_STM_DATETIME_SYNC,
    // Установка списка хостов SNTP
    IPC_COMMAND_STM_DATETIME_SNTP_HOSTS,
    
    // Не команда, база для команд, обрабатываемых модулем ESP8266
    IPC_COMMAND_ESP_HANDLE_BASE = 100,
    // Запрос настроек WIFI
    IPC_COMMAND_ESP_REQUIRE_WIFI_PARAMS,
    // Запрос списка хостов SNTP
    IPC_COMMAND_ESP_REQUIRE_SNTP_HOSTS,
};

// Тип направления
enum ipc_dir_t
{
    // Запрос
    IPC_DIR_REQUEST = 0,
    // Ответ
    IPC_DIR_RESPONSE
};

// Тип для булевы
enum ipc_bool_t
{
    IPC_BOOL_FALSE = 0,
    IPC_BOOL_TRUE,
    
    // Для количества значений
    IPC_BOOL_COUNT
};

// Структура пакета [32 байта]
ALIGN_FIELD_8
struct ipc_packet_t
{
    // Канальный слой
    struct
    {
        // Магическое число начала пакета
        uint8_t magic;
        // Байт контрольной суммы
        uint8_t checksum;
        // Номер команды
        ipc_command_t command : 8;
        // Параметры передачи
        struct
        {
            // Есть ли еще данные для этой команды
            bool more : 1;
            // Указывает что опрос нужно ускорять (есть еще команды)
            bool fast : 1;
            // Указывает направление (запрос/ответ)
            ipc_dir_t dir : 1;
            // Длинна текущих данных
            uint8_t length : 5;
        };
    } dll;
    // Прикладной слой
    union
    {
        uint8_t u8[IPC_APL_SIZE / sizeof(uint8_t)];
        uint16_t u16[IPC_APL_SIZE / sizeof(uint16_t)];
        uint32_t u32[IPC_APL_SIZE / sizeof(uint32_t)];
    } apl;

    // Копирование полей
    RAM void assign(const ipc_packet_t &source)
    {
        *this = source;
    }
};
ALIGN_FIELD_DEF

// Базовый класс данных команды
class ipc_command_data_t
{
    friend class ipc_controller_t;
protected:
    // Код команды
    const ipc_command_t command;

    // Конструктор по умолчанию
    ipc_command_data_t(ipc_command_t _command) : command(_command)
    { }

    // Получает размер буфера
    virtual size_t buffer_size(ipc_dir_t dir) const = 0;
    // Получает указатель буфера
    virtual const void * buffer_pointer(ipc_dir_t dir) const = 0;
    
    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir) const = 0;
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size) = 0;
    
    // Декодирование данных (из пакета)
    bool decode_packet(const ipc_packet_t &packet);
    // Декодирование данных (из буфера)
    bool decode_buffer(ipc_dir_t dir, const void *buffer, size_t size);
};

// Шаблон команды с фиксированным размером запроса/ответа
template <typename REQUEST, typename RESPONSE>
class ipc_command_data_template_fixed_t : public ipc_command_data_t
{
public:
    // Поля запроса
    REQUEST request;
    // Поля ответа
    RESPONSE response;

    // Получает размер буфера
    ROM virtual size_t buffer_size(ipc_dir_t dir) const
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return sizeof(request);
            case IPC_DIR_RESPONSE:
                return sizeof(response);
            default:
                return 0;
        }
    }
    
    // Получает указатель буфера
    ROM virtual const void * buffer_pointer(ipc_dir_t dir) const
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return &request;
            case IPC_DIR_RESPONSE:
                return &response;
            default:
                return NULL;
        }
    }

    // Кодирование данных, возвращает количество записанных данных
    ROM virtual size_t encode(ipc_dir_t dir) const
    {
        // Проверка данных
        assert(dir != IPC_DIR_RESPONSE || response.check());
        assert(dir != IPC_DIR_REQUEST || request.check());
        // Возвращаем размер буфера
        return buffer_size(dir);
    }
    
    // Декодирование данных
    ROM virtual bool decode(ipc_dir_t dir, size_t size)
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
            default:
                return false;
        }
        // Проверка размер данных
        return size == buffer_size(dir);
    }
protected:
    // Конструктор по умолчанию
    ipc_command_data_template_fixed_t(ipc_command_t command) : ipc_command_data_t(command)
    { }
};

// Шаблон команды оповещения (без ответа) с фиксированным размером запроса
template <typename REQUEST>
class ipc_command_data_template_notify_t : public ipc_command_data_t
{
public:
    // Поля запроса
    REQUEST request;

    // Получает размер буфера
    ROM virtual size_t buffer_size(ipc_dir_t dir) const
    {
        UNUSED(dir);
        return sizeof(request);
    }
    
    // Получает указатель буфера
    ROM virtual const void * buffer_pointer(ipc_dir_t dir) const
    {
        UNUSED(dir);
        return &request;
    }

    // Кодирование данных, возвращает количество записанных данных
    ROM virtual size_t encode(ipc_dir_t dir) const
    {
        UNUSED(dir);
        assert(dir == IPC_DIR_REQUEST);
        assert(request.check());
        // Возвращаем размер буфера
        return sizeof(request);
    }
    
    // Декодирование данных
    ROM virtual bool decode(ipc_dir_t dir, size_t size)
    {
        return 
            dir == IPC_DIR_REQUEST &&
            size == sizeof(request) &&
            request.check();
    }
protected:
    // Конструктор по умолчанию
    ipc_command_data_template_notify_t(ipc_command_t command) : ipc_command_data_t(command)
    { }
};

// Шаблон команды запроса данных (пустой запрос) с фиксированным размером ответа
template <typename RESPONSE>
class ipc_command_data_template_getter_t : public ipc_command_data_t
{
public:
    // Поля ответа
    RESPONSE response;

    // Получает размер буфера
    ROM virtual size_t buffer_size(ipc_dir_t dir) const
    {
        return (dir != IPC_DIR_RESPONSE) ? 0 : sizeof(response);
    }
    
    // Получает указатель буфера
    ROM virtual const void * buffer_pointer(ipc_dir_t dir) const
    {
        return (dir != IPC_DIR_RESPONSE) ? NULL : &response;
    }

    // Кодирование данных, возвращает количество записанных данных
    ROM virtual size_t encode(ipc_dir_t dir) const
    {
        // Проверка аргументов
        assert(dir <= IPC_DIR_RESPONSE);
        if (dir != IPC_DIR_RESPONSE)
            return 0;
        assert(response.check());
        // Возвращаем размер буфера
        return sizeof(response);
    }
    
    // Декодирование данных
    ROM virtual bool decode(ipc_dir_t dir, size_t size)
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return size == 0;
            case IPC_DIR_RESPONSE:
                return size == sizeof(response) && response.check();
            default:
                return false;
        }
    }
protected:
    // Конструктор по умолчанию
    ipc_command_data_template_getter_t(ipc_command_t command) : ipc_command_data_t(command)
    { }
};

// Поле ответа команды установки данных
ALIGN_FIELD_8
struct ipc_command_data_template_setter_response_t
{
    // Результат операции
    ipc_bool_t success : 8;
    
    // Проверка полей
    ROM bool check(void) const
    {
        return success < IPC_BOOL_COUNT;
    }
};
ALIGN_FIELD_DEF

// Шаблон команды установки данных с фиксированным ответом
template <typename RESPONSE>
class ipc_command_data_template_setter_t : public ipc_command_data_template_fixed_t<ipc_command_data_template_setter_response_t, RESPONSE>
{ };

// Поле запроса команды управления потоком
ALIGN_FIELD_8
struct ipc_command_flow_request_t
{
    // Причина ошибки
    enum reason_t
    {
        // Нет действий
        REASON_NOP = 0,
        // Сброс слотов (переполнение RX)
        REASON_OVERFLOW,
        // Сброс слотов (искажение пакета RX)
        REASON_CORRUPTION,
        // Сброс слотов (неверные данные или длинна при разборе пакета)
        REASON_BAD_CONTENT,
        
        // Для определения количества значений
        REASON_COUNT
    };
    
    // Причина ошибки
    reason_t reason : 8;
    
    // Проверка полей
    ROM bool check(void) const
    {
        return reason < REASON_COUNT;
    }
};
ALIGN_FIELD_DEF

// Класс команды управления потоком
class ipc_command_flow_t : public ipc_command_data_template_notify_t<ipc_command_flow_request_t>
{
public:
    // Конструктор по умолчанию
    ipc_command_flow_t(void) : ipc_command_data_template_notify_t(IPC_COMMAND_FLOW)
    { }
};

// Класс слота пакета
class ipc_slot_t : public ipc_packet_t, public list_item_t
{ };

// Класс списка пакетов
class ipc_slots_t
{
    // Доступные слоты пакетов
    ipc_slot_t slots[IPC_SLOT_COUNT];
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
    RAM bool empty(void) const
    {
        return used.empty();
    }
};

// Базовый класс обработчика команды
class ipc_handler_command_t : public list_item_t
{
    friend class ipc_controller_t;
protected:
    // Получает команду
    virtual ipc_command_data_t &data_get(void) = 0;
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir) = 0;
};

// Шаблон класса обработчика команды
template <typename DATA>
class ipc_handler_command_template_t : public ipc_handler_command_t
{
public:
    // Данные команды
    DATA data;
protected:
    // Получает данные команды
    ROM virtual ipc_command_data_t &data_get(void)
    {
        return data;
    }
};

// Обработчик простоя
class ipc_handler_idle_t : protected notify_t, public list_item_t
{
    friend class ipc_controller_t;
};

// Контроллер пакетов
class ipc_controller_t
{
    friend class ipc_idle_notifier_t;
    // Слоты на приём/передачу
    ipc_slots_t tx, rx;
    // Данные команды управления потоком
    ipc_command_flow_t command_flow;
    // Список обработчиков простоя
    list_template_t<ipc_handler_idle_t> idle_handlers;
    // Список обработчиков команд
    list_template_t<ipc_handler_command_t> command_handlers;
    
    // Передача команды управления потоком
    void transmit_flow(ipc_command_flow_request_t::reason_t reason);
    // Поиск обработчика по команде
    ipc_handler_command_t * handler_command_find(ipc_command_t command) const;
    // Подсчет контрольной суммы
    static uint8_t checksum(const void *source, uint8_t size);
protected:
    // Аргументы события получения данных
    struct receive_args_t
    {
        // Буфер для конечных данных
        void *buffer;
        // Печеньки для завершения обработки
        void *cookie;
        // Размер полученных данных
        const size_t size;
        
        // Конструктор по умолчанию
        receive_args_t(size_t _size) : buffer(NULL), cookie(NULL), size(_size)
        { }
    };
    
    // Обработчик события подготовки к получению данных (получение буфера и печенек)
    virtual bool receive_prepare(const ipc_packet_t &packet, receive_args_t &args);
    // Обработчик события завершения получения данных (пакеты собраны)
    virtual bool receive_finalize(const ipc_packet_t &packet, const receive_args_t &args);
    // Сброс прикладного уровня
    virtual void reset_layer(ipc_command_flow_request_t::reason_t reason, bool internal = true);
    // Добавление данных к передаче
    virtual bool transmit_raw(ipc_dir_t dir, ipc_command_t command, const void *source, size_t size);
    
    // Пуста ли очередь пакетов на отправку
    RAM bool tx_empty(void) const
    {
        return tx.empty();
    }
    
    // Пуста ли очередь пакетов на приём
    RAM bool rx_empty(void) const
    {
        return rx.empty();
    }
public:
    // Конструктор по умолчанию
    ipc_controller_t(void);
    // Сброс всех слотов (TX, RX)
    void clear_slots(void);
    
    // Добавление обработчика простоя
    void handler_add_idle(ipc_handler_idle_t &handler);
    // Добавление обработчика команд
    void handler_add_command(ipc_handler_command_t &handler);

    // Получение пакета к выводу
    void packet_output(ipc_packet_t &packet);   
    // Ввод полученного пакета
    void packet_input(const ipc_packet_t &packet);
    
    // Добавление данных к передаче(из данных команды)
    bool transmit(ipc_dir_t dir, const ipc_command_data_t &data);
    // Заполнение всех байт пакета байтом заполнения
    static void packet_clear(ipc_packet_t &packet);
};

#endif // __IPC_H
