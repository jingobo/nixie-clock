#ifndef __WEB_WS_H
#define __WEB_WS_H

#include "web_slot.h"
#include <stddef.h>
#include <stdint.h>

// ������ ������������ ���������
#define WEB_WS_HEADER_CONTROL_SIZE          2
// ������ ���� ����������� ������
#define WEB_WS_HEADER_EXTEND_LENGTH_SIZE    2
// ������ ���� �����
#define WEB_WS_HEADER_EXTEND_MASK_SIZE      4
// ������ ������������ ���������
#define WEB_WS_HEADER_EXTEND_SIZE           (WEB_WS_HEADER_EXTEND_LENGTH_SIZE + WEB_WS_HEADER_EXTEND_MASK_SIZE)
// ������ �������� ������
#define WEB_WS_PAYLOAD_SIZE                 256

// ���������� WebSocket
class web_ws_handler_t : public web_slot_handler_t
{
    // ��������� ������/������ ������
    enum state_t
    {
        STATE_CONTROL,
        STATE_LENGTH,
        STATE_MASK,
        STATE_DATA,
    };

    // ������ ������
    template <int EXTSIZE>
    struct frame_t
    {
        union
        {
            struct
            {
                // ���������
                struct
                {
                    // ����������� ���������
                    union
                    {
                        struct
                        {
                            // 1 ����
                            uint8_t code : 4;
                            uint8_t rsv : 3;
                            uint8_t fin : 1;
                            // 2 ����
                            uint8_t length : 7;
                            uint8_t mask : 1;
                        };
                        // ����� ��������
                        uint16_t raw;
                    } control;
                    // ����������� ���������
                    uint8_t extend[EXTSIZE];
                } header;
                // ������
                uint8_t payload[WEB_WS_PAYLOAD_SIZE];
            };
            // ����� ������
            uint8_t buffer[WEB_WS_HEADER_CONTROL_SIZE + EXTSIZE + WEB_WS_PAYLOAD_SIZE];
        };
    };
    // �������� �����
    struct frame_in_t : frame_t<WEB_WS_HEADER_EXTEND_SIZE>
    {
        // ��������
        struct
        {
            // ���������
            size_t state;
            // �������� ������
            size_t payload;
        } offset;
        // ��������� ����
        size_t remain;
        // ������� ���������
        state_t state;

        // ����� ���������
        void reset(void)
        {
            offset.payload = 0;
            state_set(STATE_CONTROL);
        }

        // �������� ������� �������� � ����� ��� ������
        void mask_or_data(size_t o = 0)
        {
            if (header.control.mask)
            {
                state_set(STATE_MASK, o);
                return;
            }
            memset(header.extend + WEB_WS_HEADER_EXTEND_LENGTH_SIZE, 0, WEB_WS_HEADER_EXTEND_MASK_SIZE);
            state_set(STATE_DATA, o + WEB_WS_HEADER_EXTEND_MASK_SIZE);
        }

        // ��������� ������ ���������
        void state_set(state_t value, size_t o = 0)
        {
            state = value;
            switch (value)
            {
            case STATE_CONTROL:
                remain = WEB_WS_HEADER_CONTROL_SIZE;
                offset.state = 0;
                break;
            case STATE_LENGTH:
                remain = WEB_WS_HEADER_EXTEND_LENGTH_SIZE;
                break;
            case STATE_MASK:
                remain = WEB_WS_HEADER_EXTEND_MASK_SIZE;
                offset.state += o;
                break;
            case STATE_DATA:
                // �������� ������ ������ (Big-Endian)
                remain = header.extend[0];
                remain <<= 8;
                remain |= header.extend[1];
                // �������� ������ � ������ ������
                offset.state += offset.payload + o;
                offset.payload += remain;
                break;
            default:
                assert(false);
                break;
            }
        }
    };
    // ��������� �����
    struct frame_out_t : frame_t<WEB_WS_HEADER_EXTEND_LENGTH_SIZE>
    {
        // �������� ������������ ������
        size_t offset;
        // ���������� ���������� ������ � ��������
        size_t remain;

        // ����� ���������
        void reset(void)
        {
            remain = 0;
        }

        // ����������� �� ���������
        frame_out_t(void)
        {
            header.control.raw = 0;
            header.control.fin = 1;
            header.control.mask = 0;
            header.control.rsv = 0;
        }
    };

    // ������
    struct
    {
        // ��������
        frame_in_t in;
        // ���������
        frame_out_t out;
    } frame;

    // ����� ���������
    void reset(void)
    {
        frame.in.reset();
        frame.out.reset();
    }

    // ��������� �������� ������
    void process_in(web_slot_buffer_t buffer, size_t size);
    // �������� ������
    bool transmit(uint8_t code, const void *source, size_t size);
protected:
    // ������������ �����������
    virtual void free(web_slot_free_reason_t reason);

    // ��������� ������
    virtual void execute(web_slot_buffer_t buffer);
public:
    // ����������� �� ���������
    web_ws_handler_t(void)
    {
        reset();
    }

};

// ������ ���������� ������ WebSocket ������������
template <int COUNT>
class web_ws_handler_allocator_template_t : public web_slot_handler_allocator_template_t<web_ws_handler_t, COUNT>
{ };

#endif // __WEB_WS_H
