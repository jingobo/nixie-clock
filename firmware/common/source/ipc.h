// IPC - Internal Packet Connection
#ifndef __IPC_H
#define __IPC_H

#include "list.h"

// ������ ����� ���������� ����
#define IPC_DLL_SIZE            4
// ������������ ������ ����������� ����
#define IPC_APL_SIZE            28
// ����� ������ ������
#define IPC_PKT_SIZE            (IPC_DLL_SIZE + IPC_APL_SIZE)
// �������� ������� ������
#define IPC_PKT_SIZE_CHECK()     assert(sizeof(ipc_packet_t) == IPC_PKT_SIZE)

// ���������� ������ ������� � ���� �������
#define IPC_SLOT_COUNT          10

// ��� ��� �������� �������
typedef uint8_t ipc_command_t;

// ��� �����������
enum ipc_dir_t
{
    // ������
    IPC_DIR_REQUEST = 0,
    // �����
    IPC_DIR_RESPONSE
};

// ��������� ������ [32 �����]
ALIGN_FIELD_8
struct ipc_packet_t
{
    // ��������� ����
    struct
    {
        // ���������� ����� ������ ������
        uint8_t magic;
        // ���� ����������� �����
        uint8_t checksum;
        // ����� �������
        ipc_command_t command;
        // ��������� ��������
        struct
        {
            // ���� �� ��� ������ ��� ���� �������
            bool more : 1;
            // ��������� ��� ����� ����� �������� (���� ��� �������)
            bool fast : 1;
            // ��������� ����������� (������/�����)
            ipc_dir_t dir : 1;
            // ������ ������� ������
            uint8_t length : 5;
        };
    } dll;
    // ���������� ����
    union
    {
        uint8_t u8[IPC_APL_SIZE / sizeof(uint8_t)];
        uint16_t u16[IPC_APL_SIZE / sizeof(uint16_t)];
        uint32_t u32[IPC_APL_SIZE / sizeof(uint32_t)];
    } apl;

    // ����������� �����
    RAM void assign(const ipc_packet_t &source)
    {
        *this = source;
    }
};
ALIGN_FIELD_DEF

// ������� ����� ������ �������
class ipc_command_data_t
{
    friend class ipc_controller_t;
protected:
    // ��� �������
    const ipc_command_t command;

    // ����������� �� ���������
    ipc_command_data_t(ipc_command_t _command) : command(_command)
    { }

    // �������� ������ ������
    virtual size_t buffer_size(ipc_dir_t dir) const = 0;
    // �������� ��������� ������
    virtual const void * buffer_pointer(ipc_dir_t dir) const = 0;
    
    // ����������� ������, ���������� ���������� ���������� ������
    virtual size_t encode(ipc_dir_t dir) const = 0;
    // ������������� ������
    virtual bool decode(ipc_dir_t dir, size_t size) = 0;
    
    // ������������� ������ (�� ������)
    bool decode_packet(const ipc_packet_t &packet);
    // ������������� ������ (�� ������)
    bool decode_buffer(ipc_dir_t dir, const void *buffer, size_t size);
};

// ������ ������� � ������������� �������� �������/������
template <typename REQUEST, typename RESPONSE>
class ipc_command_data_template_t : public ipc_command_data_t
{
public:
    // ���� �������
    REQUEST request;
    // ���� ������
    RESPONSE response;

    // �������� ������ ������
    ROM virtual size_t buffer_size(ipc_dir_t dir) const
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return sizeof(request);
            case IPC_DIR_RESPONSE:
                return sizeof(response);
        }
        assert(false);
        return 0;
    }
    // �������� ��������� ������
    ROM virtual const void * buffer_pointer(ipc_dir_t dir) const
    {
        switch (dir)
        {
            case IPC_DIR_REQUEST:
                return &request;
            case IPC_DIR_RESPONSE:
                return &response;
        }
        assert(false);
        return NULL;
    }

    // ����������� ������, ���������� ���������� ���������� ������
    ROM virtual size_t encode(ipc_dir_t dir) const
    {
        return buffer_size(dir);
    }
    // ������������� ������
    ROM virtual bool decode(ipc_dir_t dir, size_t size)
    {
        return buffer_size(dir) == size;
    }
protected:
    // ����������� �� ���������
    ipc_command_data_template_t(ipc_command_t command) : ipc_command_data_t(command)
    { }
};

// ����� ������� ���������� �������
class ipc_command_flow_t : public ipc_command_data_t
{
public:
    // ������� ������
    enum reason_t
    {
        // ��� ��������
        REASON_NOP = 0,
        // ����� ������ (������������ RX)
        REASON_OVERFLOW,
        // ����� ������ (��������� ������ RX)
        REASON_CORRUPTION,
        // ����� ������ (�������� ������ ��� ������ ��� ������� ������)
        REASON_BAD_CONTENT,
        
        // ��� ����������� ���������� ��������
        REASON_COUNT
    };
    
    // ���� ������� (������ �����������)
    ALIGN_FIELD_8
    struct
    {
        reason_t reason : 8;
    } request;
    ALIGN_FIELD_DEF

    // ����������� �� ���������
    ipc_command_flow_t(void) : ipc_command_data_t(0) // 0 - ��� ������� FLOW
    { }

protected:
    // �������� ������ ������
    virtual size_t buffer_size(ipc_dir_t dir) const;
    // �������� ��������� ������
    virtual const void * buffer_pointer(ipc_dir_t dir) const;

    // ����������� ������
    virtual size_t encode(ipc_dir_t dir) const;
    // ������������� ������
    virtual bool decode(ipc_dir_t dir, size_t size);
};

// ����� ����� ������
class ipc_slot_t : public ipc_packet_t, public list_item_t
{ };

// ����� ������ �������
class ipc_slots_t
{
    // ��������� ����� �������
    ipc_slot_t slots[IPC_SLOT_COUNT];
public:
    // ������ ��������� � ������������ ������
    list_template_t<ipc_slot_t> unused, used;

    // ����������� �� ���������
    ipc_slots_t(void);
    
    // ������� ����� � ������������
    void use(ipc_slot_t &slot);
    // ������� ����� � ���������
    void free(ipc_slot_t &slot);
    // �������� (������� ���� ������ � ������ ���������)
    void clear(void);
    // ����������, ����� �� ������� ������������ �������
    RAM bool empty(void) const
    {
        return used.empty();
    }
};

// ������� ����� ����������� �������
class ipc_handler_command_t : public list_item_t
{
    friend class ipc_controller_t;
protected:
    // �������� �������
    virtual ipc_command_data_t &data_get(void) = 0;
    // ���������� � ����������� ������
    virtual void notify(ipc_dir_t dir) = 0;
};

// ������ ������ ����������� �������
template <typename DATA>
class ipc_handler_command_template_t : public ipc_handler_command_t
{
public:
    // ������ �������
    DATA data;
protected:
    // �������� ������ �������
    ROM virtual ipc_command_data_t &data_get(void)
    {
        return data;
    }
};

// ���������� �������
class ipc_handler_idle_t : protected notify_t, public list_item_t
{
    friend class ipc_controller_t;
};

// ���������� �������
class ipc_controller_t
{
    friend class ipc_idle_notifier_t;
    // ����� �� ����/��������
    ipc_slots_t tx, rx;
    // ������ ������� ���������� �������
    ipc_command_flow_t command_flow;
    // ������ ������������ �������
    list_template_t<ipc_handler_idle_t> idle_handlers;
    // ������ ������������ ������
    list_template_t<ipc_handler_command_t> command_handlers;
    
    // �������� ������� ���������� �������
    void transmit_flow(ipc_command_flow_t::reason_t reason);
    // ����� ����������� �� �������
    ipc_handler_command_t * handler_command_find(ipc_command_t command) const;
    // ������� ����������� �����
    static uint8_t checksum(const void *source, uint8_t size);
protected:
    // ��������� ������� ��������� ������
    struct receive_args_t
    {
        // ����� ��� �������� ������
        void *buffer;
        // �������� ��� ���������� ���������
        void *cookie;
        // ������ ���������� ������
        const size_t size;
        
        // ����������� �� ���������
        receive_args_t(size_t _size) : buffer(NULL), cookie(NULL), size(_size)
        { }
    };
    
    // ���������� ������� ���������� � ��������� ������ (��������� ������ � �������)
    virtual bool receive_prepare(const ipc_packet_t &packet, receive_args_t &args);
    // ���������� ������� ���������� ��������� ������ (������ �������)
    virtual bool receive_finalize(const ipc_packet_t &packet, const receive_args_t &args);
    // ����� ����������� ������
    virtual void reset_layer(ipc_command_flow_t::reason_t reason, bool internal = true);
    // ���������� ������ � ��������
    virtual bool transmit_raw(ipc_dir_t dir, ipc_command_t command, const void *source, size_t size);
    
    // ����� �� ������� ������� �� ��������
    RAM bool tx_empty(void) const
    {
        return tx.empty();
    }
    
    // ����� �� ������� ������� �� ����
    RAM bool rx_empty(void) const
    {
        return rx.empty();
    }
public:
    // ����������� �� ���������
    ipc_controller_t(void);
    // ����� ���� ������ (TX, RX)
    void clear_slots(void);
    
    // ���������� ����������� �������
    void handler_add_idle(ipc_handler_idle_t &handler);
    // ���������� ����������� ������
    void handler_add_command(ipc_handler_command_t &handler);

    // ��������� ������ � ������
    void packet_output(ipc_packet_t &packet);   
    // ���� ����������� ������
    void packet_input(const ipc_packet_t &packet);
    
    // ���������� ������ � ��������(�� ������ �������)
    bool transmit(ipc_dir_t dir, const ipc_command_data_t &data);
    // ���������� ���� ���� ������ ������ ����������
    static void packet_clear(ipc_packet_t &packet);
};

#endif // __IPC_H
