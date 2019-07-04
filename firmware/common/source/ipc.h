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
    // Ложь
    IPC_BOOL_FALSE = 0,
    // Истина
    IPC_BOOL_TRUE
};

// Поддерживаемые коды команды
enum ipc_opcode_t
{
    // Команда управления потоком
    IPC_OPCODE_FLOW = 0,
    
    // Не команда, база для команд, обрабатываемых ядром STM32
    IPC_OPCODE_STM_HANDLE_BASE = 1,
        // Запрос настроек WiFi
        IPC_OPCODE_STM_WIFI_SETTINGS_GET,

    // Не команда, база для команд, обрабатываемых модулем ESP8266
    IPC_OPCODE_ESP_HANDLE_BASE = 100,
        // Оповещение, что настройки WiFi сменились
        IPC_OPCODE_ESP_WIFI_SETTINGS_CHANGED,
        
        // Запрос даты/времени из интернета
        IPC_OPCODE_ESP_TIME_GET,
        // Передача списка хостов SNTP
        IPC_OPCODE_ESP_TIME_HOSTLIST_SET,
    // Не команда, определяет лимит количества команд
    IPC_OPCODE_LIMIT = 200,
};

// Структура пакета [32 байта]
struct ipc_packet_t
{
    // Канальный слой
    ALIGN_FIELD_8
    struct
    {
        // Поле магического значения
        uint8_t magic;
        // Байт контрольной суммы
        uint8_t checksum;
        // Код команды
        ipc_opcode_t opcode : 8;
        // Параметры передачи
        struct
        {
            // Есть ли еще данные для этой команды
            ipc_bool_t more : 1;
            // Фаза передачи (для контроля пропущенного пакета)
            ipc_bool_t phase : 1;
            // Указывает направление (запрос/ответ)
            ipc_dir_t dir : 1;
            // Длинна текущих данных
            uint8_t length : 5;
        };
    } dll;
    ALIGN_FIELD_DEF
    
    // Прикладной слой
    union
    {
        uint8_t u8[IPC_APL_SIZE / sizeof(uint8_t)];
        uint16_t u16[IPC_APL_SIZE / sizeof(uint16_t)];
        uint32_t u32[IPC_APL_SIZE / sizeof(uint32_t)];
    } apl;
    
    // Заполнение всех байт пакета байтом заполнения
    void clear();
    // Заполнение всех байт apl байтом заполнения
    void clear_apl();
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

// Статус обработки пакетов
enum ipc_processor_status_t
{
    // Данные обработаны
    IPC_PROCESSOR_STATUS_SUCCESS = 0,
    // Данные пропущены
    IPC_PROCESSOR_STATUS_OVERLOOK,
    // Данные повреждены
    IPC_PROCESSOR_STATUS_CORRUPTION,
};

// Аргументы обработки данных данных
struct ipc_processor_args_t
{
    // Указывает, первый ли пакет
    bool first = true;
    // Размер полученных данных
    const size_t size;

    // Конструктор по умолчанию
    ipc_processor_args_t(size_t _size) : size(_size)
    { }
};

// Класс интерфейс процессора пакетов
class ipc_processor_t
{
public:
    // Обработка пакета
    virtual ipc_processor_status_t packet_process(const ipc_packet_t &packet, const ipc_processor_args_t &args) = 0;
    
    // Разбитие данных на пакеты
    virtual ipc_processor_status_t packet_split(ipc_opcode_t opcode, ipc_dir_t dir, const void *source, size_t size);
};

// Базовый класс обработчика событий
class ipc_event_handler_t : list_item_t
{
    friend class ipc_link_t;
protected:
    // Событие простоя
    virtual void idle(void)
    { }
    // Событие сброса
    virtual void reset(void)
    { }
};

// Базовый контроллер пакетов
class ipc_link_t : public ipc_processor_t
{
protected:
    // Причина сброса
    enum reset_reason_t
    {
        // Нет действий
        RESET_REASON_NOP = 0,
        // Переполнение слотов
        RESET_REASON_OVERFLOW,
        // Искажение данных
        RESET_REASON_CORRUPTION,
        // Неверные данные или длинна при разборе пакета
        RESET_REASON_BAD_CONTENT,
        
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
    // Список обработчиков событий
    list_template_t<ipc_event_handler_t> event_handlers;
    
    // Передача команды управления потоком
    void transmit_flow(reset_reason_t reason);
    // Подсчет контрольной суммы
    static uint8_t checksum(const void *source, uint8_t size);
public:
    // Получение пакета к выводу
    virtual void packet_output(ipc_packet_t &packet);
    // Ввод полученного пакета
    virtual void packet_input(const ipc_packet_t &packet);
    // Обработка пакета (перенос в передачу)
    virtual ipc_processor_status_t packet_process(const ipc_packet_t &packet, const ipc_processor_args_t &args);
    
    // Сброс слотов, полей
    void reset(void);
    // Добавление обработчика событий
    virtual void add_event_handler(ipc_event_handler_t &handler);
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
    virtual void reset_total(void) = 0;
    
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
public:
    // Код команды
    const ipc_opcode_t opcode;
    
    // Конструктор по умолчанию
    ipc_command_t(ipc_opcode_t _opcode) : opcode(_opcode)
    { }

    // Получает размер буфера
    virtual size_t buffer_size(ipc_dir_t dir) const = 0;
    // Получает указатель буфера
    virtual const void * buffer_pointer(ipc_dir_t dir) const = 0;
    
    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir) = 0;
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size) = 0;
    
    // Передача команды
    bool transmit(ipc_processor_t &processor, ipc_dir_t dir);
};

// Базовый класс обработчика команды
class ipc_command_handler_t : list_item_t
{
    friend class ipc_packet_glue_t;
protected:
    // Получает ссылку на команду
    virtual ipc_command_t &command_get(void) = 0;
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir) = 0;
};

// Шаблон класса обработчика команды
template <typename COMMAND>
class ipc_command_handler_template_t : public ipc_command_handler_t
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

// Шаблон класса обработчика липкой команды
template <typename COMMAND>
class ipc_command_handler_template_sticky_t : public ipc_command_handler_template_t<COMMAND>
{
    bool transmit_needed = false;
protected:
    // Обработчик передачи
    virtual bool transmit_internal(void) = 0;
public:
    // Передача данных
    void transmit(void)
    {
        transmit_needed = !transmit_internal();
    }

    // Обработчик сброса
    virtual void reset(void)
    {
        transmit_needed = false;
    }

    // Обработчик простоя
    virtual void idle(void)
    {
        if (transmit_needed)
            transmit();
    }
};

// Класс сборщика команд из пакетов
class ipc_packet_glue_t : public ipc_processor_t
{
private:
    // Данные связанные с текущей сборкой
    struct
    {
        // Смещение записи в байтах
        size_t offset;
        // Используемый обработчик
        ipc_command_handler_t *handler;
    } processing;
    // Список обработчиков
    list_template_t<ipc_command_handler_t> handlers;
    
    // Поиск обработчика по команде
    ipc_command_handler_t * find_handler(ipc_opcode_t opcode) const;
public:
    // Добавление обработчика в хост
    void add_command_handler(ipc_command_handler_t &handler);

    // Обработка пакета (склеивание в команду)
    virtual ipc_processor_status_t packet_process(const ipc_packet_t &packet, const ipc_processor_args_t &args);
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
                return 0;
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
                return NULL;
        }
    }

    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir)
    {
        // Проверка данных
        assert(dir != IPC_DIR_RESPONSE || response.check());
        assert(dir != IPC_DIR_REQUEST || request.check());
        // Возвращаем размер буфера
        return buffer_size(dir);
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
            default:
                return false;
        }
        // Проверка размер данных
        return size == buffer_size(dir);
    }
protected:
    // Конструктор по умолчанию
    ipc_command_fixed_t(ipc_opcode_t opcode) : ipc_command_t(opcode)
    { }
};

// Шаблон команды оповещения (без ответа) с фиксированным размером запроса
template <typename REQUEST>
class ipc_command_notify_t : public ipc_command_t
{
public:
    // Поля запроса
    REQUEST request;

    // Получает размер буфера
    virtual size_t buffer_size(ipc_dir_t dir) const
    {
        UNUSED(dir);
        assert(dir == IPC_DIR_REQUEST);
        return sizeof(request);
    }
    
    // Получает указатель буфера
    virtual const void * buffer_pointer(ipc_dir_t dir) const
    {
        UNUSED(dir);
        assert(dir == IPC_DIR_REQUEST);
        return &request;
    }

    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir)
    {
        assert(dir == IPC_DIR_REQUEST);
        assert(request.check());
        // Возвращаем размер буфера
        return buffer_size(dir);
    }
    
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size)
    {
        return
            dir == IPC_DIR_REQUEST &&
            size == sizeof(request) &&
            request.check();
    }
protected:
    // Конструктор по умолчанию
    ipc_command_notify_t(ipc_opcode_t opcode) : ipc_command_t(opcode)
    { }
};

// Блок для запроса/ответа нулевой длинны
ALIGN_FIELD_8
class ipc_dummy_block_t
{
    // Локальные костанты
    enum
    {
        // Проверочный код
        CHECK_CODE = 0xAA,
    };
    
    // Проверочное поле
    uint8_t dummy = CHECK_CODE;
public:
    // Проверка полей
    bool check(void) const
    {
        return dummy == CHECK_CODE;
    }
};
ALIGN_FIELD_DEF

// Шаблон команды запроса данных (пустой запрос с фиксированным размером ответа)
template <typename RESPONSE>
class ipc_command_getter_t : public ipc_command_fixed_t<ipc_dummy_block_t, RESPONSE>
{
protected:
    // Конструктор по умолчанию
    ipc_command_getter_t(ipc_opcode_t opcode)
        : ipc_command_fixed_t<ipc_dummy_block_t, RESPONSE>(opcode)
    { }
};

// Шаблон команды установки данных (пустой ответ с фиксированным запросом)
template <typename REQUEST>
class ipc_command_setter_t : public ipc_command_fixed_t<REQUEST, ipc_dummy_block_t>
{
protected:
    // Конструктор по умолчанию
    ipc_command_setter_t(ipc_opcode_t opcode) 
        : ipc_command_fixed_t<REQUEST, ipc_dummy_block_t>(opcode)
    { }
};

// Шаблон команды c пустым запросом и пустым ответом
class ipc_command_empty_t : public ipc_command_fixed_t<ipc_dummy_block_t, ipc_dummy_block_t>
{
protected:
    // Конструктор по умолчанию
    ipc_command_empty_t(ipc_opcode_t opcode)
        : ipc_command_fixed_t<ipc_dummy_block_t, ipc_dummy_block_t>(opcode)
    { }
};

// Возвращает количество символов
int ipc_string_length(const char *s, size_t size);

#endif // __IPC_H
