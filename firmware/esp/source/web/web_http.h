#ifndef __WEB_HTTP_H
#define __WEB_HTTP_H

#include "web_slot.h"
#include <romfs.h>

// ���������� ����� TODO: ��������� ���� ����
#define FILE                romfs_t::reader_t::handle_t

// ���������� HTTP ��������
class web_http_handler_t : public web_slot_handler_t
{
    // HTTP ���� �������
    enum http_status_t
    {
        // ��� ��������
        HTTP_STATUS_NA = 0,
        // ���������� �����
        HTTP_STATUS_SWITCHING_PROTOCOLS = 101,
        // ���������� �����
        HTTP_STATUS_OK = 200,
        // �� ������ ������
        HTTP_STATUS_BAD_REQUEST = 400,
        // ������ �� ������
        HTTP_STATUS_NOT_FOUND = 404,
        // ����� �� ���������������
        HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
        // ������������� ���� ������� �������
        HTTP_STATUS_URI_TOO_LONG = 414,
        // � ������
        HTTP_STATUS_IM_A_TEAPOT = 418,
        // ������������� �������� ��������
        HTTP_STATUS_UPGRADE_REQUIRED = 426,
    };

    // ��������� �������� ���� ���������� � ����������
    enum http_header_connection_t
    {
        // ����������� ��� ����������
        HTTP_HEADER_CONNECTION_UNKNOWN,
        // ������� �� ������ ��������
        HTTP_HEADER_CONNECTION_UPGRADE,
        // ��������� �������
        HTTP_HEADER_CONNECTION_CLOSED,
    };

    // ��������� �������� ��� ���� ������ ���������
    enum http_header_upgrade_t
    {
        // �������� �� ��������
        HTTP_HEADER_UPGRADE_UNKNOWN,
        // �������� WebSocket
        HTTP_HEADER_UPGRADE_WEBSOCKET,
    };

    // ����, �����������, ��� ���������� ������
    bool responsing;
    // ��������� WebSocket
    web_slot_handler_allocator_t *ws;
    // ������ �������
    class request_t
    {
        // ��������� �������
        enum
        {
            STATE_METHOD,
            STATE_PATH,
            STATE_VERSION,
            STATE_END_CR,
            STATE_END_LF,
            STATE_END_HN,
            STATE_END_HV,
        } state;
        // �������� ����� �������
        size_t end_detect;
    public:
        // ����������� ������ ����������
        struct
        {
            // �����
            char method[4];
            // ������������� ����
            char path[16];
            // ������ HTTP ��������
            char version[10];
            // �������� ��������� ������ ���������
            http_header_upgrade_t upgrade;
            // �������� ��������� ����������
            http_header_connection_t connection;
            // ��������� WebSocket
            struct
            {
                // ������
                int version;
                // ����
                char key[26];
            } websocket;
            // ��� ������ �������� ������������ ���������
            struct
            {
                // ���
                char name[22];
                // ��������
                char value[32];
            } temp;
        } headers;

        // ����� �����
        void clear(void);

        // ��������� �������
        http_status_t process(const web_slot_buffer_t data, size_t size);
    } request;
    // ������ ������
    class response_t
    {
        // �������� � ����� ������ ������������ ������
        size_t offset, total;
        // ������� ��������� ��������
        enum state_t
        {
            // ��������� ���������
            STATE_INITIAL,
            // ������ ���������
            STATE_VERSION,
            // ������ ������
            STATE_STATUS,
            // ������ ���� ������ (���������)
            STATE_CONTENT_LENGTH_HEAD,
            // ������ ���� ������ (��������)
            STATE_CONTENT_LENGTH_BODY,
            // ��� �������� (���������)
            STATE_CONTENT_TYPE_HEAD,
            // ��� �������� (��������)
            STATE_CONTENT_TYPE_BODY,
            // ��� ������������� ���������
            STATE_DYNAMIC_HEADER_NAME,
            // �������� ������������� ���������
            STATE_DYNAMIC_HEADER_VALUE,
            // ����������� ���������
            STATE_STATIC_HEADER,
            // ���� ������
            STATE_CONTENT
        } state;

        // �������� ������ � �����
        size_t send_str(web_slot_buffer_t dest, const char *source, size_t size);
    public:
        // ������� ������
        http_status_t status;
        // ������������ ����
        FILE file;
        // MIME ��� ��������� �����
        const char *mime;
        // ���������� ���������
        struct
        {
            // ������������
            struct
            {
                // ��� ��������� � ������������
                const char *name;
                // �������� � ��������� ������
                const char *value;
            } dynamic;
            // �����������
            const char *final;
        } header;

        // ����� ��������
        void clear(void);

        // ��������� ��������
        size_t process(web_slot_buffer_t dest);

        // �������� ����� �������� ������
        void process_feedback(size_t sended)
        {
            offset += sended;
            total -= sended;
        }
    } response;

    // �������� �����
    void clear(void);
    // ��������� �������, ������������� ������
    void process(web_slot_buffer_t buffer);

    // ������������ HTTP �������
    static const struct http_status_text_t
    {
        // ��� �������
        http_status_t code;
        // ����� �������
        const char * text;
    } HTTP_STATUSES[];

    // �������������� ���� ������
    static const struct content_type_t
    {
        // ���������� �����
        const char * ext;
        // MIME ���
        const char * mime;
    } CONTENT_TYPES[];

    // �������� ������ ������� HTTP �� ����
    static const char * http_status_text(http_status_t code);

    // �������� MIME ��� �� ���������� �����
    static const char * content_type_mime(const char *ext);

    // �������� ������� ����, ��������� ���������
    static bool path_check(char c);

    // ������� �������� ������ � ������ ��������
    static char * str_lower(char *s);
        
    // ������������ ������� � ��������� ������������, ��������� ���������
    static bool str_cat(char *dest, char c, size_t n);
protected:
    // ������������ �����������
    virtual void free(web_slot_free_reason_t reason);

    // ��������� ������
    virtual bool execute(web_slot_buffer_t buffer);
public:
    // ����������� �� ���������
    web_http_handler_t(void) : ws(NULL)
    {
        clear();
    }
    // ����������� � ��������� ���������� WebSocket
    web_http_handler_t(web_slot_handler_allocator_t &ws) : ws(&ws)
    {
        clear();
    }
};

// ������ ���������� ������ HTTP ������������
template <int COUNT>
class web_http_handler_allocator_template_t : public web_slot_handler_allocator_template_t<web_http_handler_t, COUNT>
{
public:
    // ����������� �� ���������
    web_http_handler_allocator_template_t(web_slot_handler_allocator_t &ws_allocator)
    {
        for (auto i = 0; i < COUNT; i++)
            this->handlers[i] = web_http_handler_t(ws_allocator);
    }
};

#endif // __WEB_HTTP_H
