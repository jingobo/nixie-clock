#include "ntime.h"
#include <sntp.h>
#include <lwip.h>
#include <core.h>
#include <log.h>
#include <os.h>

/*
#include <proto/datetime.inc>

// Имя модуля для логирования
LOG_TAG_DECL("NTIME");

// Обработчик запроса даты/времени
static class ntime_t : ipc_handler_event_t
{
    // Список хостов
    struct hosts_t
    {
        // Полученный список хостов
        datetime_hosts_data_t list;
        // Флаг, указывающий что список не получен
        bool empty;

        // Конструктор по умолчанию
        hosts_t(void) : empty(true)
        { }
    } hosts;

    // Обработчик команды синхронизации
    class handler_command_sync_t : public ipc_handler_command_template_t<datetime_command_sync_t>, public os_task_base_t
    {
        // Родительский объект
        ntime_t &ntime;

        // Попытка получения даты/времени с указанного сервера
        void try_host(const char *host)
        {
            LOGI("Try host %s", host);
            // Определение IP адреса
            sockaddr_in ep_addr;
            ep_addr.sin_family = PF_INET;
            ep_addr.sin_port = htons(123);
            {
                auto server_addr = lwip_gethostbyname(host);
                if (server_addr == NULL || server_addr->h_addr_list == NULL)
                {
                    LOGI("Resolve failed!");
                    return;
                }
                ep_addr.sin_addr = *((in_addr *)server_addr->h_addr_list[0]);
                char address[LWIP_IP_ADDRESS_BUFFER_SIZE];
                LOGI("Resolved address is %s", lwip_ip2string(ep_addr.sin_addr, address));
            }
            // Выделение сокета
            auto socket_fd = lwip_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socket_fd < 0)
            {
                LOGE("Unable to allocate socket!");
                return;
            }
            auto fail = true;
            sntp_packet_t packet;
            do
            {
                // Попытка подключения (на самом деле подключения не происходит, это же UDP)
                if (lwip_connect(socket_fd, (sockaddr *)&ep_addr, sizeof(sockaddr_in)) != 0)
                {
                    LOGI("Сonnect failed!");
                    break;
                }
                // Конфигурация сокета
                if (!lwip_socket_config(socket_fd, LOG_TAG))
                    break;
                // Отправка запроса
                if (lwip_send(socket_fd, &packet, sizeof(packet), 0) != sizeof(packet))
                {
                    LOGI("Send failed!");
                    break;
                }
                // Получение ответа
                if (lwip_recv(socket_fd, &packet, sizeof(packet), 0) < sizeof(packet))
                {
                    LOGI("Not answered!");
                    break;
                }
                fail = false;
            } while (false);
            // Закрытие сокета
            lwip_close(socket_fd);
            // Если ошибка...
            if (fail)
                return;
            // Подготовка полей
            packet.ready();
            // Проверка ответа
            if (!packet.response_check())
            {
                LOGW("Bad response!");
                return;
            }
            // Парсинг времени
            auto &dest = data.response.datetime;
            packet.time.tx.datetime_get(dest);
            // Вывод результата
            LOGI("Success. Time: %d/%d/%d %d:%d:%d", dest.day, dest.month, dest.year, dest.hour, dest.minute, dest.second);
            data.response.valid = IPC_BOOL_TRUE;
            return;
        }
    protected:
        // Оповещение о поступлении данных
        virtual void notify(ipc_dir_t dir);
        // Обработка задачи
        virtual void execute(void);
    public:
        // Конструктор по умолчанию
        handler_command_sync_t(ntime_t &parent) : os_task_base_t("time"), ntime(parent)
        { }
    } handler_command_sync;

    // Обработчик команды получения списка хостов
    class handler_command_hosts_t : public ipc_handler_command_template_t<datetime_command_hosts_t>
    {
        // Родительский объект
        ntime_t &ntime;
        // Состояние ожидания
        bool wait;
    protected:
        // Оповещение о поступлении данных
        virtual void notify(ipc_dir_t dir);
    public:
        // Конструктор по умолчанию
        handler_command_hosts_t(ntime_t &parent) : ntime(parent), wait(false)
        { }

        // Обработчик простоя
        void idle(void)
        {
            if (!ntime.hosts.empty || wait)
                return;
            datetime_command_hosts_require_t command;
            wait = core_transmit(IPC_DIR_REQUEST, command);
        }

        // Обработчик сброса
        RAM void reset(void)
        {
            wait = false;
        }
    } handler_command_hosts;
protected:
    // Событие простоя
    virtual void idle(void);
    // Событие сброса
    virtual void reset(void);
public:
    // Конструктор по умолчанию
    ntime_t(void) : handler_command_sync(*this), handler_command_hosts(*this)
    { }

    // Инициализация
    void init(void)
    {
        core_handler_add_event(*this);
        core_handler_add_command(handler_command_sync);
        core_handler_add_command(handler_command_hosts);
    }
} ntime;

void ntime_t::idle(void)
{
    // Базовый метод
    ipc_handler_event_t::idle();
    // Если нужно получить список серверов
    handler_command_hosts.idle();
}

void ntime_t::reset(void)
{
    // Сброс окманд
    handler_command_hosts.reset();
    // Базовый метод
    ipc_handler_event_t::reset();
}

void ntime_t::handler_command_sync_t::notify(ipc_dir_t dir)
{
    // Можем обрабатьывать только запрос
    assert(dir == IPC_DIR_REQUEST);
    // Запуск задачи синхронизации
    LOGI("Request datetime...");
    if (!start())
        LOGW("Already executing...");
}

void ntime_t::handler_command_sync_t::execute(void)
{
    // Сброс полей ответа
    data.response.datetime.clear();
    data.response.valid = IPC_BOOL_FALSE;
    // Обработка
    do
    {
        // Проверка, что список получен
        if (ntime.hosts.empty)
        {
            LOGW("Sync canceled, hosts list is not fetched!");
            break;
        }
        // Копирование списка
        ntime.handler_command_sync.mutex.enter();
            datetime_hosts_data_t list;
            datetime_hosts_data_copy(list, ntime.hosts.list);
        ntime.handler_command_sync.mutex.leave();
        // Обход списка
        auto host_start = list;
        for (auto i = 0; i < sizeof(datetime_hosts_data_t); i++)
        {
            auto c = list[i];
            // Если перевод строки
            if (c == '\n')
            {
                list[i] = '\0';
                try_host(host_start);
                if (data.response.valid)
                    break;
                host_start = list + i + 1;
                continue;
            }
            // Если конец списка
            if (c == '\0')
            {
                if (i != 0)
                    try_host(host_start);
                break;
            }
        }
    } while (false);
    // Передача ответа
    core_transmit(IPC_DIR_RESPONSE, data);
}

void ntime_t::handler_command_hosts_t::notify(ipc_dir_t dir)
{
    // Можем обрабатывать только запрос
    assert(dir == IPC_DIR_REQUEST);
    // Получаем список
    ntime.handler_command_sync.mutex.enter();
        datetime_hosts_data_copy(ntime.hosts.list, data.request.hosts);
    ntime.handler_command_sync.mutex.leave();
    ntime.hosts.empty = false;
    LOGI("Hosts fetched");
}
*/
void ntime_init(void)
{
    //ntime.init();
}
