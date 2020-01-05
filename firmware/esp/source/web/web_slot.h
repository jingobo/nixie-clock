#ifndef __WEB_SLOT_H
#define __WEB_SLOT_H

#include <os.h>
#include <lwip.h>

// Буфер данных
typedef uint8_t web_slot_buffer_t[CONFIG_TCP_SND_BUF_DEFAULT];

// Причина освобождения слота
enum web_slot_free_reason_t
{
    // Нормальное закрытие с нашей стороны
    WEB_SLOT_FREE_REASON_INSIDE,
    // Нормальное закрытие с чужой стороны
    WEB_SLOT_FREE_REASON_OUTSIDE,
    // Закрытие по причине ошибки сети
    WEB_SLOT_FREE_REASON_NETWORK,
    // Переход на другой обработчик/протокол
    WEB_SLOT_FREE_REASON_MIGRATION,
};

// Предварительное объявление
class web_slot_socket_t;

// Обработчик протокола
class web_slot_handler_t
{
    // Дружеский класс слота сокета
    friend class web_slot_socket_t;
protected:
    // Используемый слот сокета
    web_slot_socket_t *socket = NULL;

    // Получает, занят ли обработчик
    bool busy(void) const
    {
        return socket != NULL;
    }

    // Освобождение обработчика
    virtual void free(web_slot_free_reason_t reason)
    {
        assert(busy());
        // Освобождение
        socket = NULL;
    }

    // Обработка данных
    virtual void execute(web_slot_buffer_t buffer) = 0;
public:
    // Выделение обработчика
    virtual bool allocate(web_slot_socket_t &socket)
    {
        if (busy())
            return false;
        this->socket = &socket;
        return true;
    }
};

// Базовый класс алокатора слотов обработчиков
class web_slot_handler_allocator_t
{
public:
    // Выделение слота обработчика
    virtual web_slot_handler_t * allocate(web_slot_socket_t &slot) = 0;
};

// Шаблон алокатора слотов обработчиков
template <typename HANDLER, int COUNT>
class web_slot_handler_allocator_template_t : public web_slot_handler_allocator_t
{
protected:
    // Список слотов обработчиков
    HANDLER handlers[COUNT];
public:
    // Выделение слота обработчика
    virtual web_slot_handler_t * allocate(web_slot_socket_t &socket) override final
    {
        for (int i = 0; i < COUNT; i++)
            if (handlers[i].allocate(socket))
                return handlers + i;
        return NULL;
    }
};

// Слот для сокета
class web_slot_socket_t
{
    // BSD Сокет
    lwip_socket_t socket = LWIP_INVALID_SOCKET;
    // Текущий слот обработчика
    web_slot_handler_t *handler = NULL;
    // Закрывается ли в данный момент соединение
    bool closing = false;
    // Адрес удаленной стороны
    char address[LWIP_IP_ADDRESS_BUFFER_SIZE + 6];
    // Таймаут
    struct
    {
        // Время начала
        os_tick_t start;
        // Период срабатывания
        os_tick_t period;
    } timeout;

    // Закрывает соединение
    void close(void);
    // Определение нормального закрытия сокета
    void close_detect(void);

    // Проверяет результат операции ввода/вывода
    int32_t check_io(int32_t fb);
public:
    // Получает, занят ли слот
    bool busy(void) const
    {
        return socket != LWIP_INVALID_SOCKET;
    }

    // Вывод лога для теущего клиента
    void log(const char *format, ...) const;

    // Сброс таймаута
    void timeout_reset(void);
    // Смена таймаута
    void timeout_change(uint32_t mills);

    // Выделение слота
    bool allocate(web_slot_handler_allocator_t &initial_allocator, lwip_socket_t socket);
    // Смена обработчика сокета
    void handler_change(web_slot_handler_t *new_handler, web_slot_free_reason_t reason);
    // Освобождение слота
    void free(web_slot_free_reason_t reason);

    // Приём данных
    int32_t read(web_slot_buffer_t buffer);
    // Передача данных
    int32_t write(const web_slot_buffer_t buffer, int size);

    // Обработка слота
    void execute(web_slot_buffer_t buffer);
};

// Базовый класс аллокатора слотов сокетов
class web_slot_socket_allocator_t
{
public:
    // Выделение слота сокета
    virtual web_slot_socket_t * allocate(lwip_socket_t s) = 0;
    // Обработка всех слотов
    virtual void execute(web_slot_buffer_t buffer) = 0;
    // Получает количество выделенных слотов
    virtual uint8_t allocated_count(void) const = 0;
};

// Шаблон алокатора слотов сокетов
template <int COUNT>
class web_slot_socket_allocator_template_t : public web_slot_socket_allocator_t
{
    // Список слотов сокетов
    web_slot_socket_t sockets[COUNT];
    // Аллокатор слотов обработчиков
    web_slot_handler_allocator_t &allocator;
public:
    // Конструктор по умолчанию
    web_slot_socket_allocator_template_t(web_slot_handler_allocator_t &_allocator)
        : allocator(_allocator)
    { }

    // Выделение слота сокета
    virtual web_slot_socket_t * allocate(lwip_socket_t socket) override final
    {
        for (int i = 0; i < COUNT; i++)
            if (sockets[i].allocate(allocator, socket))
                return sockets + i;
        return NULL;
    }

    // Обработка всех слотов
    virtual void execute(web_slot_buffer_t buffer) override final
    {
        for (auto i = 0; i < COUNT; i++)
            sockets[i].execute(buffer);
    }

    // Получает количество выделенных слотов
    virtual uint8_t allocated_count(void) const override final
    {
        uint8_t result = 0;
        for (auto i = 0; i < COUNT; i++)
            if (sockets[i].busy())
                result++;
        return result;
    }
};

#endif // __WEB_SLOT_H
