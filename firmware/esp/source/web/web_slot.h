#ifndef __WEB_SLOT_H
#define __WEB_SLOT_H

#include <os.h>
#include <lwip.h>

// ����� ������
typedef uint8_t web_slot_buffer_t[CONFIG_TCP_SND_BUF_DEFAULT];

// ������� ������������ �����
enum web_slot_free_reason_t
{
    // ���������� �������� � ����� �������
    WEB_SLOT_FREE_REASON_INSIDE,
    // ���������� �������� � ����� �������
    WEB_SLOT_FREE_REASON_OUTSIDE,
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
    web_slot_socket_t *socket = NULL;

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
    virtual void execute(web_slot_buffer_t buffer) = 0;
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
    virtual web_slot_handler_t * allocate(web_slot_socket_t &socket) override final
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
    lwip_socket_t socket = LWIP_INVALID_SOCKET;
    // ������� ���� �����������
    web_slot_handler_t *handler = NULL;
    // ����������� �� � ������ ������ ����������
    bool closing = false;
    // ����� ��������� �������
    char address[LWIP_IP_ADDRESS_BUFFER_SIZE + 6];
    // �������
    struct
    {
        // ����� ������
        os_tick_t start;
        // ������ ������������
        os_tick_t period;
    } timeout;

    // ��������� ����������
    void close(void);
    // ����������� ����������� �������� ������
    void close_detect(void);

    // ��������� ��������� �������� �����/������
    int32_t check_io(int32_t fb);
public:
    // ��������, ����� �� ����
    bool busy(void) const
    {
        return socket != LWIP_INVALID_SOCKET;
    }

    // ����� ���� ��� ������� �������
    void log(const char *format, ...) const;

    // ����� ��������
    void timeout_reset(void);
    // ����� ��������
    void timeout_change(uint32_t mills);

    // ��������� �����
    bool allocate(web_slot_handler_allocator_t &initial_allocator, lwip_socket_t socket);
    // ����� ����������� ������
    void handler_change(web_slot_handler_t *new_handler, web_slot_free_reason_t reason);
    // ������������ �����
    void free(web_slot_free_reason_t reason);

    // ���� ������
    int32_t read(web_slot_buffer_t buffer);
    // �������� ������
    int32_t write(const web_slot_buffer_t buffer, int size);

    // ��������� �����
    void execute(web_slot_buffer_t buffer);
};

// ������� ����� ���������� ������ �������
class web_slot_socket_allocator_t
{
public:
    // ��������� ����� ������
    virtual web_slot_socket_t * allocate(lwip_socket_t s) = 0;
    // ��������� ���� ������
    virtual void execute(web_slot_buffer_t buffer) = 0;
    // �������� ���������� ���������� ������
    virtual uint8_t allocated_count(void) const = 0;
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
    web_slot_socket_allocator_template_t(web_slot_handler_allocator_t &_allocator)
        : allocator(_allocator)
    { }

    // ��������� ����� ������
    virtual web_slot_socket_t * allocate(lwip_socket_t socket) override final
    {
        for (int i = 0; i < COUNT; i++)
            if (sockets[i].allocate(allocator, socket))
                return sockets + i;
        return NULL;
    }

    // ��������� ���� ������
    virtual void execute(web_slot_buffer_t buffer) override final
    {
        for (auto i = 0; i < COUNT; i++)
            sockets[i].execute(buffer);
    }

    // �������� ���������� ���������� ������
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
