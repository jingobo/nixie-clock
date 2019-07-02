#include "wifi.h"
#include "httpd.h"

#include <os.h>
#include <log.h>
#include <lwip.h>

#include <web/web_ws.h>
#include <web/web_slot.h>
#include <web/web_http.h>

// Имя модуля для логирования
LOG_TAG_DECL("HTTPD");

// Максимальное количество клиентов
#define HTTPD_MAX_ALL_SOCKETS       CONFIG_LWIP_MAX_ACTIVE_TCP
// Максимальное количество WEB-сокетов
#define HTTPD_MAX_WEB_SOCKETS       1
// Максимальное количество HTTO-сокетов
#define HTTPD_MAX_HTTP_SOCKETS      (HTTPD_MAX_ALL_SOCKETS - HTTPD_MAX_WEB_SOCKETS)

// Аллокатор слоток WS обработчиков
static web_ws_handler_allocator_template_t<HTTPD_MAX_WEB_SOCKETS> httpd_ws_handlers;
// Аллокатор слотов HTTP обработчиков
static web_http_handler_allocator_template_t<HTTPD_MAX_HTTP_SOCKETS> httpd_web_handlers(httpd_ws_handlers);
// Аллокатор слотов сокетов
static web_slot_socket_allocator_template_t<HTTPD_MAX_ALL_SOCKETS> httpd_socket_handlers(httpd_web_handlers);

// Задача обработки клиентских сокетов
static class httpd_client_task_t : public os_task_base_t
{
    // Очередь новых сокетов
    const xQueueHandle queue;
    // Для временного хранения нового сокета
    lwip_socket_t socket;
    // Промежуточный буфер для обработки данных
    web_slot_buffer_t buffer;
protected:
    // Обработчик задачи
    virtual void execute(void)
    {
        uint8_t allocated_last = UINT8_MAX;
        // Обработка
        for (;;)
        {

            // Пробуем достать нового клиента
            if (xQueueReceive(queue, &socket, OS_MS_TO_TICKS(20)) > 0)
                if (httpd_socket_handlers.allocate(socket) == NULL)
                {
                    // Закрытие сокета
                    lwip_close(socket);
                    // Лог
                    LOGW("Free client slot not found!");
                }
            // Обработка текущих клиентов
            httpd_socket_handlers.execute(buffer);
            // Вывод количества выделенных слотов
            auto allocated = httpd_socket_handlers.allocated_count();
            if (allocated_last != allocated)
            {
                allocated_last = allocated;
                LOGI("Slot count: %d", allocated);
            }
        }
    }
public:
    // Конструктор по умолчанию
    httpd_client_task_t(void) :
        os_task_base_t("httpd-client"),
        queue(xQueueCreate(HTTPD_MAX_ALL_SOCKETS, sizeof(lwip_socket_t)))
    {
        assert(OS_CHECK_HANDLE(queue));
    }

    // Добавление нового клиента в обработку
    void new_client(lwip_socket_t socket)
    {
        // Проверка аргументов
        assert(socket > LWIP_INVALID_SOCKET);
        // Добавление в очередь
        while (!xQueueSend(queue, &socket, OS_TICK_MAX))
        { }
    }
} httpd_client_task;

// Задача обработки серверного сокета
static class httpd_server_task_t : public os_task_base_t
{
    // Серверный сокет
    lwip_socket_t server_fd;

    // Задержка на одну секунду
    void dealy(void)
    {
        delay(OS_MS_TO_TICKS(1000));
    }

    // Внетренняя обработка задачи с уже созданным сокетом
    void execute_internal(void)
    {
        // Привязка к адресу
        sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        // Инициализация адреса
        sa.sin_family = AF_INET;
        sa.sin_port = htons(80);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        // Привязка
        if (lwip_bind(server_fd, (sockaddr *)&sa, sizeof(sa)) < 0)
        {
            auto error = errno;
            LOGW("Bind on port 80 failed: %d", error);
            if (error != EADDRINUSE)
                return;
            // Если уже занят адрес - не решаемая на данный момент проблема
            esp_restart();
        }
        // Старт прослушки порта
        if (lwip_listen(server_fd, MIN(HTTPD_MAX_HTTP_SOCKETS, 2)) < 0)
        {
            LOGE("Listen failed: %d", errno);
            return;
        }
        LOGI("Started...");
        // Получение клиентов
        for (;;)
        {
            auto client_fd = lwip_accept(server_fd, NULL, NULL);
            if (client_fd <= LWIP_INVALID_SOCKET)
            {
                auto error = errno;
                LOGW("Accept failed: %d", error);
                if (error != ENFILE && error != EAGAIN)
                    return;
                // Возможно через 1 секунду сокеты освободятся
                dealy();
                continue;
            }
            // Установка не блокирующего режима
            if (!lwip_socket_nbio(client_fd))
            {
                lwip_close(client_fd);
                return;
            }
            // Добавление нового сокета
            httpd_client_task.new_client(client_fd);
        }
    }
protected:
    // Обработчик задачи
    virtual void execute(void)
    {
        // Обработка
        for (;;)
        {
            // Инициализация серверного сокета
            server_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (server_fd > LWIP_INVALID_SOCKET)
            {
                // Обработка
                execute_internal();
                // Освобождение сокета
                lwip_close(server_fd);
            }
            else
                LOGE("Error opening server socket: %d", errno);
            // Рестарт через 1 секунду
            LOGW("Restarting...");
            dealy();
        }
    }
public:
    // Конструктор по умолчанию
    httpd_server_task_t(void) : os_task_base_t("httpd-server")
    { }
} httpd_server_task;

void httpd_init(void)
{
    // Запуск клиентской задачи
    httpd_client_task.start();
    assert(httpd_client_task.running());
    // Запуск серверной задачи
    httpd_server_task.start();
    assert(httpd_server_task.running());
}
