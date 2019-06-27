#ifndef __WEB_SLOT_H
#define __WEB_SLOT_H

#include <common.h>
#include <lwip.h>
#include <log.h>

// Буфер данных
typedef uint8_t web_slot_buffer_t[1024];

// Причина освобождения слота
enum web_slot_free_reason_t
{
    // Нормальное закрытие с нашей стороны
    WEB_SLOT_FREE_REASON_NORMAL,
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
    web_slot_socket_t *socket;

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
    virtual bool execute(web_slot_buffer_t buffer)
    {
        return busy();
    }

    // Конструктор по умолчанию
    web_slot_handler_t(void) : socket(NULL)
    { }
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
    virtual web_slot_handler_t * allocate(web_slot_socket_t &socket)
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
    lwip_socket_t socket;
    // Адрес удаленной стороны
    char address[LWIP_IP_ADDRESS_BUFFER_SIZE + 4];
    // Текущий слот обработчика
    web_slot_handler_t *handler;

    // Получает, занят ли слот
    bool busy(void) const
    {
        return socket != LWIP_INVALID_SOCKET;
    }
public:
    // Конструктор по умолчанию
    web_slot_socket_t(void) : socket(LWIP_INVALID_SOCKET), handler(NULL)
    { }

    // Вывод лога для теущего клиента
    void log(const char *format, ...)const
    {
        assert(busy());
        va_list args;
        va_start(args, format);
            esp_log_write_va(ESP_LOG_INFO, address, format, args);
        va_end(args);
    }

    // Выделение слота
    bool allocate(web_slot_handler_allocator_t &initial_allocator, lwip_socket_t socket)
    {
        assert(socket > LWIP_INVALID_SOCKET);
        // Занят ли слот
        if (busy())
            return false;
        // Инициализация полей
        this->socket = socket;
        // Получаем удаленный адрес
        sockaddr_in addr;
        auto len = (socklen_t)sizeof(addr);
        lwip_getpeername(socket, (struct sockaddr*)&addr, &len);
        lwip_ip2string(addr.sin_addr, address);
        sprintf(address + strlen(address), ":%d", ntohs(addr.sin_port));
        // Вывод в лог
        log("Connected!");
        // Выделение обработчика
        handler_change(initial_allocator.allocate(*this), WEB_SLOT_FREE_REASON_MIGRATION);
        // Проверка обработчика
        if (handler == NULL)
        {
            log("Unable to allocate initial handler slot!");
            free(WEB_SLOT_FREE_REASON_NORMAL);
        }
        return true;
    }

    // Смена обработчика сокета
    void handler_change(web_slot_handler_t *new_handler, web_slot_free_reason_t reason)
    {
        if (handler != NULL)
            handler->free(reason);
        handler = new_handler;
    }

    // Освобождение слота
    void free(web_slot_free_reason_t reason)
    {
        assert(busy());
        // Закрытие соекта
        lwip_close(socket);
        // Вывод в лог
        switch (reason)
        {
        case WEB_SLOT_FREE_REASON_NORMAL:
            log("Closed!");
            break;
        case WEB_SLOT_FREE_REASON_NETWORK:
            log("Disconnected!");
            break;
        default:
            assert(false);
            break;
        }
        // Освобождение обработчика
        handler_change(NULL, reason);
        // Освобождение слота
        socket = LWIP_INVALID_SOCKET;
    }

    // Обработка слота
    bool execute(web_slot_buffer_t buffer)
    {
        // Занят ли слот
        if (!busy())
            return false;
        return handler->execute(buffer);
    }

    // Приём данных
    int read(web_slot_buffer_t buffer)
    {
        assert(buffer != NULL);
        // Чтение
        auto result = lwip_recv(socket, buffer, sizeof(web_slot_buffer_t), 0);
        // Если соедиение закрыто
        if (result == 0)
            free(WEB_SLOT_FREE_REASON_NETWORK);
        return result;
    }

    // Передача данных
    int write(const web_slot_buffer_t buffer, int size)
    {
        assert(size > 0);
        // Запись
        auto result = lwip_send(socket, buffer, size, 0);
        // Если соедиение закрыто
        if (result == 0)
            free(WEB_SLOT_FREE_REASON_NETWORK);
        return result;
    }
};

// Базовый класс аллокатора слотов сокетов
class web_slot_socket_allocator_t
{
public:
    // Выделение слота сокета
    virtual web_slot_socket_t * allocate(lwip_socket_t s) = 0;
    // Обработка всех слотов
    virtual bool execute(web_slot_buffer_t buffer) = 0;
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
    web_slot_socket_allocator_template_t(web_slot_handler_allocator_t &http_allocator)
        : allocator(http_allocator)
    { }

    // Выделение слота сокета
    virtual web_slot_socket_t * allocate(lwip_socket_t socket)
    {
        for (int i = 0; i < COUNT; i++)
            if (sockets[i].allocate(allocator, socket))
                return sockets + i;
        return NULL;
    }

    // Обработка всех слотов
    virtual bool execute(web_slot_buffer_t buffer)
    {
        auto empty = true;
        for (auto i = 0; i < COUNT; i++)
            if (sockets[i].execute(buffer))
                empty = false;
        return empty;
    }
};

#endif // __WEB_SLOT_H
