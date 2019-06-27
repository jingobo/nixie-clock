#ifndef __WEB_SLOT_H
#define __WEB_SLOT_H

#include <common.h>
#include <lwip.h>
#include <log.h>

// ����� ������
typedef uint8_t web_slot_buffer_t[1024];

// ������� ������������ �����
enum web_slot_free_reason_t
{
    // ���������� �������� � ����� �������
    WEB_SLOT_FREE_REASON_NORMAL,
    // �������� �� ������� ������ ����
    WEB_SLOT_FREE_REASON_NETWORK,
    // ������� �� ������ ����������/��������
    WEB_SLOT_FREE_REASON_MIGRATION,
};

// ��������������� ����������
class web_slot_socket_t;

// ���������� ���������
class web_slot_handler_t
{
    // ��������� ����� ����� ������
    friend class web_slot_socket_t;

protected:
    // ������������ ���� ������
    web_slot_socket_t *socket;

    // ��������, ����� �� ����������
    bool busy(void) const
    {
        return socket != NULL;
    }

    // ������������ �����������
    virtual void free(web_slot_free_reason_t reason)
    {
        assert(busy());
        // ������������
        socket = NULL;
    }

    // ��������� ������
    virtual bool execute(web_slot_buffer_t buffer)
    {
        return busy();
    }

    // ����������� �� ���������
    web_slot_handler_t(void) : socket(NULL)
    { }
public:
    // ��������� �����������
    virtual bool allocate(web_slot_socket_t &socket)
    {
        if (busy())
            return false;
        this->socket = &socket;
        return true;
    }
};

// ������� ����� ��������� ������ ������������
class web_slot_handler_allocator_t
{
public:
    // ��������� ����� �����������
    virtual web_slot_handler_t * allocate(web_slot_socket_t &slot) = 0;
};

// ������ ��������� ������ ������������
template <typename HANDLER, int COUNT>
class web_slot_handler_allocator_template_t : public web_slot_handler_allocator_t
{
protected:
    // ������ ������ ������������
    HANDLER handlers[COUNT];
public:
    // ��������� ����� �����������
    virtual web_slot_handler_t * allocate(web_slot_socket_t &socket)
    {
        for (int i = 0; i < COUNT; i++)
            if (handlers[i].allocate(socket))
                return handlers + i;
        return NULL;
    }
};

// ���� ��� ������
class web_slot_socket_t
{
    // BSD �����
    lwip_socket_t socket;
    // ����� ��������� �������
    char address[LWIP_IP_ADDRESS_BUFFER_SIZE + 4];
    // ������� ���� �����������
    web_slot_handler_t *handler;

    // ��������, ����� �� ����
    bool busy(void) const
    {
        return socket != LWIP_INVALID_SOCKET;
    }
public:
    // ����������� �� ���������
    web_slot_socket_t(void) : socket(LWIP_INVALID_SOCKET), handler(NULL)
    { }

    // ����� ���� ��� ������� �������
    void log(const char *format, ...)const
    {
        assert(busy());
        va_list args;
        va_start(args, format);
            esp_log_write_va(ESP_LOG_INFO, address, format, args);
        va_end(args);
    }

    // ��������� �����
    bool allocate(web_slot_handler_allocator_t &initial_allocator, lwip_socket_t socket)
    {
        assert(socket > LWIP_INVALID_SOCKET);
        // ����� �� ����
        if (busy())
            return false;
        // ������������� �����
        this->socket = socket;
        // �������� ��������� �����
        sockaddr_in addr;
        auto len = (socklen_t)sizeof(addr);
        lwip_getpeername(socket, (struct sockaddr*)&addr, &len);
        lwip_ip2string(addr.sin_addr, address);
        sprintf(address + strlen(address), ":%d", ntohs(addr.sin_port));
        // ����� � ���
        log("Connected!");
        // ��������� �����������
        handler_change(initial_allocator.allocate(*this), WEB_SLOT_FREE_REASON_MIGRATION);
        // �������� �����������
        if (handler == NULL)
        {
            log("Unable to allocate initial handler slot!");
            free(WEB_SLOT_FREE_REASON_NORMAL);
        }
        return true;
    }

    // ����� ����������� ������
    void handler_change(web_slot_handler_t *new_handler, web_slot_free_reason_t reason)
    {
        if (handler != NULL)
            handler->free(reason);
        handler = new_handler;
    }

    // ������������ �����
    void free(web_slot_free_reason_t reason)
    {
        assert(busy());
        // �������� ������
        lwip_close(socket);
        // ����� � ���
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
        // ������������ �����������
        handler_change(NULL, reason);
        // ������������ �����
        socket = LWIP_INVALID_SOCKET;
    }

    // ��������� �����
    bool execute(web_slot_buffer_t buffer)
    {
        // ����� �� ����
        if (!busy())
            return false;
        return handler->execute(buffer);
    }

    // ���� ������
    int read(web_slot_buffer_t buffer)
    {
        assert(buffer != NULL);
        // ������
        auto result = lwip_recv(socket, buffer, sizeof(web_slot_buffer_t), 0);
        // ���� ��������� �������
        if (result == 0)
            free(WEB_SLOT_FREE_REASON_NETWORK);
        return result;
    }

    // �������� ������
    int write(const web_slot_buffer_t buffer, int size)
    {
        assert(size > 0);
        // ������
        auto result = lwip_send(socket, buffer, size, 0);
        // ���� ��������� �������
        if (result == 0)
            free(WEB_SLOT_FREE_REASON_NETWORK);
        return result;
    }
};

// ������� ����� ���������� ������ �������
class web_slot_socket_allocator_t
{
public:
    // ��������� ����� ������
    virtual web_slot_socket_t * allocate(lwip_socket_t s) = 0;
    // ��������� ���� ������
    virtual bool execute(web_slot_buffer_t buffer) = 0;
};

// ������ ��������� ������ �������
template <int COUNT>
class web_slot_socket_allocator_template_t : public web_slot_socket_allocator_t
{
    // ������ ������ �������
    web_slot_socket_t sockets[COUNT];
    // ��������� ������ ������������
    web_slot_handler_allocator_t &allocator;
public:
    // ����������� �� ���������
    web_slot_socket_allocator_template_t(web_slot_handler_allocator_t &http_allocator)
        : allocator(http_allocator)
    { }

    // ��������� ����� ������
    virtual web_slot_socket_t * allocate(lwip_socket_t socket)
    {
        for (int i = 0; i < COUNT; i++)
            if (sockets[i].allocate(allocator, socket))
                return sockets + i;
        return NULL;
    }

    // ��������� ���� ������
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
