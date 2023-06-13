#include "wifi.h"
#include "httpd.h"

#include <os.h>
#include <log.h>
#include <stm.h>
#include <lwip.h>
#include <core.h>

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

// Размер опкода IPC в потоке WS в байтах
#define HTTPD_WS_IPC_OPCODE_SIZE    1

// Состояния буфера IPC
enum httpd_ipc_state_t
{
    // Данных к передаче нет
    HTTPD_IPC_STATE_IDLE,
    // Передать ошибку
    HTTPD_IPC_STATE_ERROR,
    // Есть нормальные данные
    HTTPD_IPC_STATE_NORMAL,
};

// Данные между веб сокетом и системой IPC
static struct
{
    // Промежуточный буфер
    uint8_t buffer[WEB_WS_PAYLOAD_SIZE];
    // Размер заполдненных данных в буфере
    size_t size;
    // Мьютекс синхронизации
    os_mutex_t mutex;
    // Состояние буфера
    httpd_ipc_state_t state = HTTPD_IPC_STATE_IDLE;
} httpd_ipc_data;

// Класс реализации обработчиков чтения/записи веб сокетов
class httpd_ws_handler_t : public web_ws_handler_t
{
protected:
    // Событие передачи данных (бинарных)
    virtual void transmit_event(void) override final
    {
        // Синхронизация
        httpd_ipc_data.mutex.enter();
            switch (httpd_ipc_data.state)
            {
                case HTTPD_IPC_STATE_IDLE:
                    // Простой
                    break;
                case HTTPD_IPC_STATE_ERROR:
                    // Ошибка
                    {
                        const ipc_opcode_t opcode = IPC_OPCODE_FLOW;
                        if (transmit(&opcode, sizeof(opcode)))
                            httpd_ipc_data.state = HTTPD_IPC_STATE_IDLE;
                    }
                    break;
                case HTTPD_IPC_STATE_NORMAL:
                    // Ответ
                    if (transmit(httpd_ipc_data.buffer, httpd_ipc_data.size))
                        httpd_ipc_data.state = HTTPD_IPC_STATE_IDLE;
                    break;
                default:
                    assert(false);
            }
        httpd_ipc_data.mutex.leave();
    }

    // Событие приёма данных (бинарных)
    virtual void receive_event(const uint8_t *data, size_t size) override final
    {
        // Размер должен быть минимум 1 байт (опкод)
        if (size < HTTPD_WS_IPC_OPCODE_SIZE)
        {
            socket->log("Short packet size: %d!", size);
            return;
        }
        // Передача в IPC
        // Проверка команды
        auto opcode = (ipc_opcode_t)data[0];
        if (opcode >= IPC_OPCODE_LIMIT)
        {
            socket->log("Invalid opcode: %d!", opcode);
            return;
        }
        // Разбитие данных на пакеты IPC, синхронизация мьютексом не требуется
        httpd_ipc_data.state = ipc_processor_t::data_split(core_processor_out.web, opcode, IPC_DIR_REQUEST, data + HTTPD_WS_IPC_OPCODE_SIZE, size - HTTPD_WS_IPC_OPCODE_SIZE) ?
                HTTPD_IPC_STATE_IDLE :
                HTTPD_IPC_STATE_ERROR;
    }
public:
    // Выделение обработчика
    virtual bool allocate(web_slot_socket_t &socket) override final
    {
        auto result = web_ws_handler_t::allocate(socket);
        if (result)
            // Сброс передаваемых данных
            httpd_ipc_data.state = HTTPD_IPC_STATE_IDLE;
        return result;
    }
};

// Аллокатор слотов WS обработчиков
static web_slot_handler_allocator_template_t<httpd_ws_handler_t, HTTPD_MAX_WEB_SOCKETS> httpd_ws_handlers;
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
    virtual void execute(void) override final
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
                LOGH();
            }
        }
    }
public:
    // Конструктор по умолчанию
    httpd_client_task_t(void) :
        os_task_base_t("httpd-client", true),
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
        memory_clear(&sa, sizeof(sa));

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
        if (lwip_listen(server_fd, minimum(HTTPD_MAX_HTTP_SOCKETS, 2)) < 0)
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
    virtual void execute(void) override final
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
    httpd_server_task_t(void) : os_task_base_t("httpd-server", true)
    { }
} httpd_server_task;

// Процессор входящих пактов для Web
ipc_processor_proxy_t httpd_processor_in([](const ipc_packet_t &packet, const ipc_processor_t::args_t &args)
{
    // Если первый пакет...
    if (args.first)
    {
        // Проверяем, поместятся ли данные
        if (args.size > WEB_WS_PAYLOAD_SIZE - HTTPD_WS_IPC_OPCODE_SIZE)
            return false;
        // Сброс флага готовности
        httpd_ipc_data.mutex.enter();
            httpd_ipc_data.state = HTTPD_IPC_STATE_IDLE;
        httpd_ipc_data.mutex.leave();
        // Добавляем байт команды
        httpd_ipc_data.size = HTTPD_WS_IPC_OPCODE_SIZE;
        httpd_ipc_data.buffer[0] = (uint8_t)packet.dll.opcode;
    }
    // Проверяем условия
    assert(httpd_ipc_data.state != HTTPD_IPC_STATE_NORMAL);
    assert(packet.dll.opcode == httpd_ipc_data.buffer[0]);
    // Добавялем данные
    auto len = packet.dll.length;
    memcpy(httpd_ipc_data.buffer + httpd_ipc_data.size, &packet.apl, len);
    httpd_ipc_data.size += len;
    // Если пакет последний
    if (!packet.dll.more)
    {
        // Проверяем условия
        assert(args.size == httpd_ipc_data.size - HTTPD_WS_IPC_OPCODE_SIZE);
        // Установка флага готовности
        httpd_ipc_data.mutex.enter();
            httpd_ipc_data.state = HTTPD_IPC_STATE_NORMAL;
        httpd_ipc_data.mutex.leave();
    }
    return true;
});

void httpd_init(void)
{
    // Запуск клиентской и серверной задачи
    httpd_client_task.start();
    httpd_server_task.start();
}
