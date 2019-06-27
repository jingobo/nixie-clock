#include "httpd.h"

#include <os.h>
#include <log.h>
#include <lwip.h>

#include <web/web_ws.h>
#include <web/web_slot.h>
#include <web/web_http.h>

// Максимальное количество клиентов
#define HTTPD_MAX_ALL_SOCKETS       10
// Максимальное количество WEB-сокетов
#define HTTPD_MAX_WEB_SOCKETS       1
// Максимальное количество HTTO-сокетов
#define HTTPD_MAX_HTTP_SOCKETS      (HTTPD_MAX_ALL_SOCKETS - HTTPD_MAX_WEB_SOCKETS)

// Имя модуля для логирования
LOG_TAG_DECL("HTTPD");

// Класс веб сервера
static class httpd_t
{
    // Аллокатор слоток WS обработчиков
    //web_ws_handler_allocator_template_t<HTTPD_MAX_WEB_SOCKETS> ws;
    // Аллокатор слотов HTTP обработчиков
    //web_http_handler_allocator_template_t<HTTPD_MAX_HTTP_SOCKETS> http;
    // Аллокатор слотов сокетов
    //web_slot_socket_allocator_template_t<HTTPD_MAX_ALL_SOCKETS> sockets;

    // Задача обработки серверного сокета
    class task_server_t : public os_task_base_t
    {
        // Серверный сокет
        lwip_socket_t server_fd;

        // Внетренняя обработка задачи с уже созданным сокетом
        void execute_internal(void)
        {
            int result;
            // Привязка к адресу
            {
                sockaddr_in sa;
                memset(&sa, 0, sizeof(sa));
                // Инициализация адреса
                sa.sin_family = AF_INET;
                sa.sin_port = htons(80);
                sa.sin_addr.s_addr = htonl(INADDR_ANY);
                // Привязка
                result = lwip_bind(server_fd, (sockaddr *)&sa, sizeof(sa));
                if (result < 0)
                {
                    LOGW("Bind on port 80 failed: %d", result);
                    return;
                }
            }
            // Старт прослушки порта
            result = lwip_listen(server_fd, HTTPD_MAX_ALL_SOCKETS);
            if (result < 0)
            {
                LOGE("Listen failed: %d", result);
                return;
            }
            // Получение клиентов
            for (;;)
            {
                auto client_fd = lwip_accept(server_fd, NULL, NULL);
                if (client_fd <= LWIP_INVALID_SOCKET)
                {
                    LOGW("Accept failed: %d", result);
                    return;
                }
                // Установка не блокирующего режима
                if (!lwip_socket_nbio(client_fd, LOG_TAG))
                {
                    lwip_close(client_fd);
                    return;
                }
                // Добавление нового сокета
                lwip_close(client_fd);
            }
        }
    protected:
        // Обработчик задачи
        virtual void execute(void)
        {
            // Проверка состояния
            assert(server_fd <= LWIP_INVALID_SOCKET);
            // Инициализация серверного сокета
            auto server_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (server_fd < 0)
            {
                LOGE("Error opening server socket: %d", server_fd);
                return;
            }
            // Обработка
            execute_internal();
            // Закрытие серверного сокета
            lwip_close(server_fd);
            server_fd = LWIP_INVALID_SOCKET;
        }
    public:
        // Конструктор по умолчанию
        task_server_t(void) : os_task_base_t("httpd_server"), server_fd(LWIP_INVALID_SOCKET)
        { }
    } task_server;
public:
    // Конструктор по умолчанию
    httpd_t(void) //: http(ws), sockets(http)
    { }

    // Старт веб сервера
    void start(void)
    {
        task_server.start();
    }
} httpd;

void httpd_init(void)
{
    httpd.start();
}
