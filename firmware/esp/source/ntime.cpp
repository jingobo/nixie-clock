#include "ntime.h"
#include "sntp.h"
#include "lwip.h"
#include "task.h"
#include <core.h>

#include <proto/datetime.inc>

// ��� ������ ��� �����������
#define MODULE_NAME     "NTIME"
#define NTIME_SERVER    "0.pool.ntp.org"
//#define NTIME_SERVER    "192.168.0.106"

// ���������� ������� ����/�������
static class ntime_t : public ipc_handler_command_template_t<command_datetime_t>, task_wrapper_t
{
    // ������� ��������� ����/������� � ���������� �������
    bool fetch(const char *host);
protected:
    // ��������� ������
    virtual void execute(void);
    // ���������� � ����������� ������
    virtual void notify(ipc_dir_t dir);
public:
    ntime_t(void) : task_wrapper_t("time")
    { }
} ntime;

// ���������� � ����������� ������
ROM void ntime_t::notify(ipc_dir_t dir)
{
    assert(dir == IPC_DIR_REQUEST);
    log_module(MODULE_NAME, "Request datetime...");
    if (!start())
        log_module(MODULE_NAME, "Already executing...");
}

// ������� ��������� ����/������� � ���������� �������
ROM bool ntime_t::fetch(const char *host)
{
    log_module(MODULE_NAME, "Try host %s", host);
    // ����������� IP ������
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
    // ��������� ������
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
        // ������� ����������� (�� ����� ���� ����������� �� ����������, ��� �� UDP)
        if (connect(socket_fd, (sockaddr *)&ep_addr, sizeof(sockaddr_in)) != 0)
        {
            log_module(MODULE_NAME, "�onnect failed!");
            break;
        }
        // ��������� �������� �������� ������ � 100 ��
        int tv = 100;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
        {
            log_module(MODULE_NAME, "Unable to set read timeout!");
            break;
        }
        // �������� �������
        if (send(socket_fd, &packet, sizeof(packet), 0) != sizeof(packet))
        {
            log_module(MODULE_NAME, "Send failed!");
            break;
        }
        // ��������� ������
        if (recv(socket_fd, &packet, sizeof(packet), 0) < sizeof(packet))
        {
            log_module(MODULE_NAME, "Not answered!");
            break;
        }
        result = true;
    } while (false);
    // �������� ������
    close(socket_fd);
    // ���� ������...
    if (!result)
        return false;
    // ���������� �����
    packet.ready();
    // �������� ������
    if (!packet.response_check())
    {
        log_module(MODULE_NAME, "Bad response!");
        return false;
    }
    // ������� �������
    auto &dest = data.response.datetime;
    packet.time.tx.datetime_get(dest);
    // ����� ����������
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
