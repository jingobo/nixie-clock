#include "ntime.h"
#include "sntp.h"
#include "lwip.h"
#include "task.h"
#include <core.h>

#include <proto/datetime.inc>

// Имя модуля для логирования
#define MODULE_NAME     "NTIME"
#define NTIME_SERVER    "0.pool.ntp.org"
//#define NTIME_SERVER    "192.168.0.106"

// Обработчик запроса даты/времени
static class ntime_t : public ipc_handler_command_template_t<command_datetime_t>, task_wrapper_t
{
    // Попытка получения даты/времени с указанного сервера
    bool fetch(const char *host);
protected:
    // Обработка задачи
    virtual void execute(void);
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir);
public:
    ntime_t(void) : task_wrapper_t("time")
    { }
} ntime;

// Оповещение о поступлении данных
ROM void ntime_t::notify(ipc_dir_t dir)
{
    assert(dir == IPC_DIR_REQUEST);
    log_module(MODULE_NAME, "Request datetime...");
    if (!start())
        log_module(MODULE_NAME, "Already executing...");
}

// Попытка получения даты/времени с указанного сервера
ROM bool ntime_t::fetch(const char *host)
{
    log_module(MODULE_NAME, "Try host %s", host);
    // Определение IP адреса
    sockaddr_in ep_addr;
    ep_addr.sin_family = PF_INET;
    ep_addr.sin_port = htons(123);
    {
        auto server_addr = gethostbyname(NTIME_SERVER);
        if (server_addr == NULL || server_addr->h_addr_list == NULL)
        {
            log_module(MODULE_NAME, "Resolve failed!");
            return false;
        }
        ep_addr.sin_addr = *((in_addr *)server_addr->h_addr_list[0]);
        log_module(MODULE_NAME, "Resolved address is %s", inet_ntoa(ep_addr.sin_addr));
    }
    // Выделение сокета
    auto socket_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0)
    {
        log_module(MODULE_NAME, "Unable to allocate socket!");
        return false;
    }
    auto result = false;
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
        result = true;
    } while (false);
    // Закрытие сокета
    close(socket_fd);
    // Если ошибка...
    if (!result)
        return false;
    // Подготовка полей
    packet.ready();
    // Проверка ответа
    if (!packet.response_check())
    {
        log_module(MODULE_NAME, "Bad response!");
        return false;
    }
    // Парсинг времени
    auto &dest = data.response.datetime;
    packet.time.tx.datetime_get(dest);
    // Вывод результата
    log_module(MODULE_NAME, "Success. Time: %d/%d/%d %d:%d:%d", dest.day, dest.month, dest.year, dest.hour, dest.minute, dest.second);
    data.response.quality = command_datetime_response_t::QUALITY_SUCCESS;
    return true;
}

ROM void ntime_t::execute(void)
{
    data.response.quality = command_datetime_response_t::QUALITY_NETWORK;
    ntime.fetch(NTIME_SERVER);
    core_transmit(IPC_DIR_RESPONSE, data);
}

ROM void ntime_init(void)
{
    core_handler_add_command(ntime);
}
