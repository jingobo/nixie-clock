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

// Список хостов
struct ntime_hosts_t
{
    // Флаг, указывающий что список не получен
    bool empty = true;
    // Полученный список хостов
    time_hosts_data_t list;
    // Мьютекс синхронизации списка
    os_mutex_t mutex;
} ntime_hosts;

// Обработчик команды получения даты/времени
static class ntime_command_time_sync_t : public ipc_command_handler_template_sticky_t<time_command_sync_t>
{
    // Флаг состояния обработки
    bool working = false;
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir);

    // Передача данных
    virtual bool transmit_internal(void)
    {
        return command.transmit(core_processor_out.esp, IPC_DIR_RESPONSE);
    }
public:
    // Обратная связь после обработки
    void feedback(void)
    {
        // Передача результата
        transmit();
        // Отработали
        core_main_task.mutex.enter();
            working = false;
        core_main_task.mutex.leave();
    }
} ntime_command_time_sync;

// Попытка получения даты/времени с указанного сервера
static void ntime_try_host(const char *host, datetime_t &dest)
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
    if (!packet.time.tx.datetime_get(dest))
        return;
    // Вывод результата
    LOGI("Success. Time: %d/%d/%d %d:%d:%d (GMT 0)", dest.day, dest.month, dest.year, dest.hour, dest.minute, dest.second);
    ntime_command_time_sync.command.response.status = time_command_sync_response_t::STATUS_SUCCESS;
}

// Попытка получения даты/времени со всех хостов
static void ntime_try_hosts(void)
{
    // Проверяем, получен ли список
    if (ntime_hosts.empty)
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
    // Обход списка
    auto size = 0;
    char host[TIME_HOSTANME_CHARS_MAX];
    for (auto i = 0; i < sizeof(time_hosts_data_t); i++)
    {
        auto c = ntime_hosts.list[i];
        // Если перевод строки
        if (c == '\n')
        {
            host[size] = '\0';
            // Попытка запроса по указанному хосту
            ntime_try_host(host, ntime_command_time_sync.command.response.value);
            // Если успех = выходим
            if (ntime_command_time_sync.command.response.status == time_command_sync_response_t::STATUS_SUCCESS)
                break;
            // Переход к следующему хосту
            size = 0;
            continue;
        }
        // Если конец списка
        if (c == '\0')
        {
            host[size] = '\0';
            // Если не первый символ...
            // Попытка запроса по указанному хосту
            ntime_try_host(host, ntime_command_time_sync.command.response.value);
            break;
        }
        host[size++] = c;
    }
}

void ntime_command_time_sync_t::notify(ipc_dir_t dir)
{
    // Мы можем только обрабатывать запрос
    assert(dir == IPC_DIR_REQUEST);
    // Проверка на обработку
    LOGI("Request datetime...");
    core_main_task.mutex.enter();
        if (working)
        {
            LOGW("Already executing!");
            core_main_task.mutex.leave();
            return;
        }
        working = true;
    core_main_task.mutex.leave();
    // Запуск задачи на синхронизацию
    core_main_task.deffered_call([](void)
    {
        // Обработка хостов
        ntime_hosts.mutex.enter();
            ntime_try_hosts();
        ntime_hosts.mutex.leave();
        // Обратная связь
        ntime_command_time_sync.feedback();
    });
}

// Обработчик команды запроса списка SNTP хостов
static class ntime_command_hostlist_set_t : public ipc_command_handler_template_sticky_t<time_command_hostlist_set_t>
{
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать запрос
        assert(dir == IPC_DIR_REQUEST);
        // Сохраняем список хостов
        ntime_hosts.mutex.enter();
            time_hosts_data_copy(ntime_hosts.list, command.request.hosts);
            ntime_hosts.empty = time_hosts_empty(ntime_hosts.list);
        ntime_hosts.mutex.leave();
        LOGI("Hosts fetched");
        // Передача ответа
        transmit();
    }

    // Передача данных
    virtual bool transmit_internal(void)
    {
        return command.transmit(core_processor_out.esp, IPC_DIR_RESPONSE);
    }
} ntime_command_hostlist_set;

// Класс обработчика событий IPC
static class ntime_ipc_handler_t : public ipc_event_handler_t
{
protected:
    // Событие простоя
    virtual void idle(void)
    {
        // Базовый метод
        ipc_event_handler_t::idle();
        // Команды
        ntime_command_time_sync.idle();
        ntime_command_hostlist_set.idle();
    }

    // Событие сброса
    virtual void reset(void)
    {
        // Базовый метод
        ipc_event_handler_t::reset();
        // Команды
        ntime_command_time_sync.reset();
        ntime_command_hostlist_set.reset();
    }
} ntime_ipc_handler;

void ntime_init(void)
{
    stm_add_event_handler(ntime_ipc_handler);
    core_add_command_handler(ntime_command_time_sync);
    core_add_command_handler(ntime_command_hostlist_set);
}
