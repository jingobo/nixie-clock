#include "httpd.h"

#include <os.h>
#include <log.h>
#include <lwip.h>

#include <web/web_ws.h>
#include <web/web_slot.h>
#include <web/web_http.h>

// ������������ ���������� ��������
#define HTTPD_MAX_ALL_SOCKETS       10
// ������������ ���������� WEB-�������
#define HTTPD_MAX_WEB_SOCKETS       1
// ������������ ���������� HTTO-�������
#define HTTPD_MAX_HTTP_SOCKETS      (HTTPD_MAX_ALL_SOCKETS - HTTPD_MAX_WEB_SOCKETS)

// ��� ������ ��� �����������
LOG_TAG_DECL("HTTPD");

// ����� ��� �������
static class httpd_t
{
    // ��������� ������ WS ������������
    //web_ws_handler_allocator_template_t<HTTPD_MAX_WEB_SOCKETS> ws;
    // ��������� ������ HTTP ������������
    //web_http_handler_allocator_template_t<HTTPD_MAX_HTTP_SOCKETS> http;
    // ��������� ������ �������
    //web_slot_socket_allocator_template_t<HTTPD_MAX_ALL_SOCKETS> sockets;

    // ������ ��������� ���������� ������
    class task_server_t : public os_task_base_t
    {
        // ��������� �����
        lwip_socket_t server_fd;

        // ���������� ��������� ������ � ��� ��������� �������
        void execute_internal(void)
        {
            int result;
            // �������� � ������
            {
                sockaddr_in sa;
                memset(&sa, 0, sizeof(sa));
                // ������������� ������
                sa.sin_family = AF_INET;
                sa.sin_port = htons(80);
                sa.sin_addr.s_addr = htonl(INADDR_ANY);
                // ��������
                result = lwip_bind(server_fd, (sockaddr *)&sa, sizeof(sa));
                if (result < 0)
                {
                    LOGW("Bind on port 80 failed: %d", result);
                    return;
                }
            }
            // ����� ��������� �����
            result = lwip_listen(server_fd, HTTPD_MAX_ALL_SOCKETS);
            if (result < 0)
            {
                LOGE("Listen failed: %d", result);
                return;
            }
            // ��������� ��������
            for (;;)
            {
                auto client_fd = lwip_accept(server_fd, NULL, NULL);
                if (client_fd <= LWIP_INVALID_SOCKET)
                {
                    LOGW("Accept failed: %d", result);
                    return;
                }
                // ��������� �� ������������ ������
                if (!lwip_socket_nbio(client_fd, LOG_TAG))
                {
                    lwip_close(client_fd);
                    return;
                }
                // ���������� ������ ������
                lwip_close(client_fd);
            }
        }
    protected:
        // ���������� ������
        virtual void execute(void)
        {
            // �������� ���������
            assert(server_fd <= LWIP_INVALID_SOCKET);
            // ������������� ���������� ������
            auto server_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (server_fd < 0)
            {
                LOGE("Error opening server socket: %d", server_fd);
                return;
            }
            // ���������
            execute_internal();
            // �������� ���������� ������
            lwip_close(server_fd);
            server_fd = LWIP_INVALID_SOCKET;
        }
    public:
        // ����������� �� ���������
        task_server_t(void) : os_task_base_t("httpd_server"), server_fd(LWIP_INVALID_SOCKET)
        { }
    } task_server;
public:
    // ����������� �� ���������
    httpd_t(void) //: http(ws), sockets(http)
    { }

    // ����� ��� �������
    void start(void)
    {
        task_server.start();
    }
} httpd;

void httpd_init(void)
{
    httpd.start();
}
