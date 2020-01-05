#include <os.h>
#include <stm.h>
#include <log.h>
#include <sntp.h>
#include <lwip.h>
#include <core.h>
#include "wifi.h"
#include "ntime.h"

#include <proto/time.inc>

// Имя модуля для логирования
LOG_TAG_DECL("NTIME");

// Класс загрузчика сетевого времени
static class ntime_loader_t : public os_task_base_t
{
protected:
    // Обработчик задачи
    virtual void execute(void) override final;
public:
    // Список хостов
    struct ntime_hosts_t
    {
        // Полученный список хостов
        time_hosts_data_t list;
        // Временная копия списока хостов
        time_hosts_data_t copy;
        // Флаг, указывающий что список не получен
        bool empty = true;
    } hosts;

    // Конструктор по умолчанию
    ntime_loader_t(void) : os_task_base_t("ntime")
    { }
} ntime_loader;

// Обработчик команды получения даты/времени
static class ntime_command_time_sync_t : public ipc_responder_template_t<time_command_sync_t>
{
    // Флаг состояния обработки
    bool executing = false;
protected:
    // Обработка данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;

        if (ntime_loader.running())
            // Задача еще выполняется
            return;

        if (executing)
        {
            // Задача была обработана
            transmit();
            executing = false;
            return;
        }

        // Запуск задачи
        LOGI("Request datetime...");
        executing = ntime_loader.start();
    }
} ntime_command_time_sync;

// Попытка получения даты/времени с указанного сервера
static bool ntime_try_host(const char *host)
{
    if (strlen(host) <= 0)
        return false;
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
            return false;
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
        return false;
    }

    // Основной цикл
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
        if (!lwip_socket_bio(socket_fd))
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
    if (fail)
        return false;

    // Проверка ответа
    if (!packet.ready())
    {
        LOGW("Bad response!");
        return false;
    }

    // Парсинг времени
    auto &dest = ntime_command_time_sync.command.response.value;
    if (!packet.time.tx.datetime_get(dest))
        return false;
    // Вывод результата
    LOGI("Success. Time: %d/%d/%d %d:%d:%d (GMT 0)", dest.day, dest.month, dest.year, dest.hour, dest.minute, dest.second);
    ntime_command_time_sync.command.response.status = time_command_sync_response_t::STATUS_SUCCESS;
    return true;
}

void ntime_loader_t::execute(void)
{
    // Проверяем, получен ли список
    if (hosts.empty)
    {
        LOGW("Sync canceled, hosts list is not fetched!");
        ntime_command_time_sync.command.response.status = time_command_sync_response_t::STATUS_HOSTLIST;
        return;
    }
    ntime_command_time_sync.command.response.status = time_command_sync_response_t::STATUS_FAILED;

    // Проверяем, есть ли какой либо сетевой интерфейс
    if (!wifi_wait())
    {
        LOGW("Sync canceled, no network interfaces!");
        return;
    }

    // Локальная копия
    mutex.enter();
        time_hosts_data_copy(ntime_loader.hosts.copy, ntime_loader.hosts.list);
    mutex.leave();

    // Обход списка
    auto hosts = ntime_loader.hosts.copy;
    for (auto host = hosts;; hosts++)
        switch (*hosts)
        {
            // Если перевод строки
            case '\n':
                *hosts = '\0';
                // Попытка запроса по указанному хосту
                if (ntime_try_host(host))
                    return;
                // Переход к следующему хосту
                host = hosts + 1;
                break;

            // Если конец списка
            case '\0':
                *hosts = '\0';
                // Попытка запроса по указанному хосту
                ntime_try_host(host);
                return;
        }
}

// Обработчик команды запроса списка SNTP хостов
static class ntime_command_hostlist_set_t : public ipc_responder_template_t<time_command_hostlist_set_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;

        // Сохраняем список хостов
        ntime_loader.mutex.enter();
            time_hosts_data_copy(ntime_loader.hosts.list, command.request.hosts);
            ntime_loader.hosts.empty = time_hosts_empty(ntime_loader.hosts.list);
        ntime_loader.mutex.leave();
        LOGI("Hosts fetched");

        // Передача ответа
        transmit();
    }
} ntime_command_hostlist_set;

void ntime_init(void)
{
    core_handler_add(ntime_command_time_sync);
    core_handler_add(ntime_command_hostlist_set);
}
