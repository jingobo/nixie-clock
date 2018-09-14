#include "ntime.h"
#include "sntp.h"
#include "lwip.h"
#include "task.h"
#include <core.h>

#include <proto/datetime.inc>

// Имя модуля для логирования
#define MODULE_NAME     "NTIME"

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
    class handler_command_sync_t : public ipc_handler_command_template_t<datetime_command_sync_t>, public task_wrapper_t
    {
        // Родительский объект
        ntime_t &ntime;

        // Попытка получения даты/времени с указанного сервера
        ROM void try_host(const char *host)
        {
            log_module(MODULE_NAME, "Try host %s", host);
            // Определение IP адреса
            sockaddr_in ep_addr;
            ep_addr.sin_family = PF_INET;
            ep_addr.sin_port = htons(123);
            {
                auto server_addr = gethostbyname(host);
                if (server_addr == NULL || server_addr->h_addr_list == NULL)
                {
                    log_module(MODULE_NAME, "Resolve failed!");
                    return;
                }
                ep_addr.sin_addr = *((in_addr *)server_addr->h_addr_list[0]);
                log_module(MODULE_NAME, "Resolved address is %s", inet_ntoa(ep_addr.sin_addr));
            }
            // Выделение сокета
            auto socket_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socket_fd < 0)
            {
                log_module(MODULE_NAME, "Unable to allocate socket!");
                return;
            }
            auto fail = true;
            sntp_packet_t packet;
            do
            {
                // Попытка подключения (на самом деле подключения не происходит, это же UDP)
                if (connect(socket_fd, (sockaddr *)&ep_addr, sizeof(sockaddr_in)) != 0)
                {
                    log_module(MODULE_NAME, "Сonnect failed!");
                    break;
                }
                // Установка таймаута ожидания данных в 100 мС
                int tv = 100;
                if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
                {
                    log_module(MODULE_NAME, "Unable to set read timeout!");
                    break;
                }
                // Отправка запроса
                if (send(socket_fd, &packet, sizeof(packet), 0) != sizeof(packet))
                {
                    log_module(MODULE_NAME, "Send failed!");
                    break;
                }
                // Получение ответа
                if (recv(socket_fd, &packet, sizeof(packet), 0) < sizeof(packet))
                {
                    log_module(MODULE_NAME, "Not answered!");
                    break;
                }
                fail = false;
            } while (false);
            // Закрытие сокета
            close(socket_fd);
            // Если ошибка...
            if (fail)
                return;
            // Подготовка полей
            packet.ready();
            // Проверка ответа
            if (!packet.response_check())
            {
                log_module(MODULE_NAME, "Bad response!");
                return;
            }
            // Парсинг времени
            auto &dest = data.response.datetime;
            packet.time.tx.datetime_get(dest);
            // Вывод результата
            log_module(MODULE_NAME, "Success. Time: %d/%d/%d %d:%d:%d", dest.day, dest.month, dest.year, dest.hour, dest.minute, dest.second);
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
        handler_command_sync_t(ntime_t &parent) : task_wrapper_t("time"), ntime(parent)
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
        ROM void idle(void)
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
    ROM void init(void)
    {
        core_handler_add_event(*this);
        core_handler_add_command(handler_command_sync);
        core_handler_add_command(handler_command_hosts);
    }
} ntime;

ROM void ntime_t::idle(void)
{
    // Базовый метод
    ipc_handler_event_t::idle();
    // Если нужно получить список серверов
    handler_command_hosts.idle();
}

ROM void ntime_t::reset(void)
{
    // Сброс окманд
    handler_command_hosts.reset();
    // Базовый метод
    ipc_handler_event_t::reset();
}

ROM void ntime_t::handler_command_sync_t::notify(ipc_dir_t dir)
{
    // Можем обрабатьывать только запрос
    assert(dir == IPC_DIR_REQUEST);
    // Запуск задачи синхронизации
    log_module(MODULE_NAME, "Request datetime...");
    if (!start())
        log_module(MODULE_NAME, "Already executing...");
}

ROM void ntime_t::handler_command_sync_t::execute(void)
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
            log_module(MODULE_NAME, "Sync canceled, hosts list is not fetched!");
            break;
        }
        // Копирование списка
        MUTEX_ENTER(ntime.handler_command_sync.mutex);
            datetime_hosts_data_t list;
            datetime_hosts_data_copy(list, ntime.hosts.list);
        MUTEX_LEAVE(ntime.handler_command_sync.mutex);
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

ROM void ntime_t::handler_command_hosts_t::notify(ipc_dir_t dir)
{
    // Можем обрабатывать только запрос
    assert(dir == IPC_DIR_REQUEST);
    // Получаем список
    MUTEX_ENTER(ntime.handler_command_sync.mutex);
        datetime_hosts_data_copy(ntime.hosts.list, data.request.hosts);
    MUTEX_LEAVE(ntime.handler_command_sync.mutex);
    ntime.hosts.empty = false;
    log_module(MODULE_NAME, "Hosts fetched");
}

RAM void ntime_init(void)
{
    ntime.init();
}
