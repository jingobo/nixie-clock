#include "wifi.h"
#include "httpd.h"

#include <os.h>
#include <log.h>
#include <lwip.h>

#include <web/web_ws.h>
#include <web/web_slot.h>
#include <web/web_http.h>

// ��� ������ ��� �����������
LOG_TAG_DECL("HTTPD");

// ������������ ���������� ��������
#define HTTPD_MAX_ALL_SOCKETS       CONFIG_LWIP_MAX_ACTIVE_TCP
// ������������ ���������� WEB-�������
#define HTTPD_MAX_WEB_SOCKETS       1
// ������������ ���������� HTTO-�������
#define HTTPD_MAX_HTTP_SOCKETS      (HTTPD_MAX_ALL_SOCKETS - HTTPD_MAX_WEB_SOCKETS)

// ��������� ������ WS ������������
static web_ws_handler_allocator_template_t<HTTPD_MAX_WEB_SOCKETS> httpd_ws_handlers;
// ��������� ������ HTTP ������������
static web_http_handler_allocator_template_t<HTTPD_MAX_HTTP_SOCKETS> httpd_web_handlers(httpd_ws_handlers);
// ��������� ������ �������
static web_slot_socket_allocator_template_t<HTTPD_MAX_ALL_SOCKETS> httpd_socket_handlers(httpd_web_handlers);

// ������ ��������� ���������� �������
static class httpd_client_task_t : public os_task_base_t
{
    // ������� ����� �������
    const xQueueHandle queue;
    // ��� ���������� �������� ������ ������
    lwip_socket_t socket;
    // ������������� ����� ��� ��������� ������
    web_slot_buffer_t buffer;
protected:
    // ���������� ������
    virtual void execute(void)
    {
        uint8_t allocated_last = UINT8_MAX;
        // ���������
        for (;;)
        {

            // ������� ������� ������ �������
            if (xQueueReceive(queue, &socket, OS_MS_TO_TICKS(20)) > 0)
                if (httpd_socket_handlers.allocate(socket) == NULL)
                {
                    // �������� ������
                    lwip_close(socket);
                    // ���
                    LOGW("Free client slot not found!");
                }
            // ��������� ������� ��������
            httpd_socket_handlers.execute(buffer);
            // ����� ���������� ���������� ������
            auto allocated = httpd_socket_handlers.allocated_count();
            if (allocated_last != allocated)
            {
                allocated_last = allocated;
                LOGI("Slot count: %d", allocated);
            }
        }
    }
public:
    // ����������� �� ���������
    httpd_client_task_t(void) :
        os_task_base_t("httpd-client"),
        queue(xQueueCreate(HTTPD_MAX_ALL_SOCKETS, sizeof(lwip_socket_t)))
    {
        assert(OS_CHECK_HANDLE(queue));
    }

    // ���������� ������ ������� � ���������
    void new_client(lwip_socket_t socket)
    {
        // �������� ����������
        assert(socket > LWIP_INVALID_SOCKET);
        // ���������� � �������
        while (!xQueueSend(queue, &socket, OS_TICK_MAX))
        { }
    }
} httpd_client_task;

// ������ ��������� ���������� ������
static class httpd_server_task_t : public os_task_base_t
{
    // ��������� �����
    lwip_socket_t server_fd;

    // �������� �� ���� �������
    void dealy(void)
    {
        delay(OS_MS_TO_TICKS(1000));
    }

    // ���������� ��������� ������ � ��� ��������� �������
    void execute_internal(void)
    {
        // �������� � ������
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        // ������������� ������
        sa.sin_family = AF_INET;
        sa.sin_port = htons(80);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        // ��������
        if (lwip_bind(server_fd, (sockaddr *)&sa, sizeof(sa)) < 0)
        {
            auto error = errno;
            LOGW("Bind on port 80 failed: %d", error);
            if (error != EADDRINUSE)
                return;
            // ���� ��� ����� ����� - �� �������� �� ������ ������ ��������
            esp_restart();
        }
        // ����� ��������� �����
        if (lwip_listen(server_fd, MIN(HTTPD_MAX_HTTP_SOCKETS, 2)) < 0)
        {
            LOGE("Listen failed: %d", errno);
            return;
        }
        LOGI("Started...");
        // ��������� ��������
        for (;;)
        {
            auto client_fd = lwip_accept(server_fd, NULL, NULL);
            if (client_fd <= LWIP_INVALID_SOCKET)
            {
                auto error = errno;
                LOGW("Accept failed: %d", error);
                if (error != ENFILE && error != EAGAIN)
                    return;
                // �������� ����� 1 ������� ������ �����������
                dealy();
                continue;
            }
            // ��������� �� ������������ ������
            if (!lwip_socket_nbio(client_fd))
            {
                lwip_close(client_fd);
                return;
            }
            // ���������� ������ ������
            httpd_client_task.new_client(client_fd);
        }
    }
protected:
    // ���������� ������
    virtual void execute(void)
    {
        // ���������
        for (;;)
        {
            // ������������� ���������� ������
            server_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (server_fd > LWIP_INVALID_SOCKET)
            {
                // ���������
                execute_internal();
                // ������������ ������
                lwip_close(server_fd);
            }
            else
                LOGE("Error opening server socket: %d", errno);
            // ������� ����� 1 �������
            LOGW("Restarting...");
            dealy();
        }
    }
public:
    // ����������� �� ���������
    httpd_server_task_t(void) : os_task_base_t("httpd-server")
    { }
} httpd_server_task;

void httpd_init(void)
{
    // ������ ���������� ������
    httpd_client_task.start();
    assert(httpd_client_task.running());
    // ������ ��������� ������
    httpd_server_task.start();
    assert(httpd_server_task.running());
}
