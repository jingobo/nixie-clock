#include <io.h>
#include <log.h>
#include "web_slot.h"

void web_slot_socket_t::log(const char *format, ...) const
{
    assert(busy());
    va_list args;
    va_start(args, format);
        esp_log_write_va(ESP_LOG_INFO, address, format, args);
    va_end(args);
}

void web_slot_socket_t::timeout_reset(void)
{
    timeout.start = os_tick_get();
}

void web_slot_socket_t::timeout_change(uint32_t mills)
{
    timeout_reset();
    timeout.period = OS_MS_TO_TICKS(mills);
}

bool web_slot_socket_t::allocate(web_slot_handler_allocator_t &initial_allocator, lwip_socket_t socket)
{
    assert(socket > LWIP_INVALID_SOCKET);
    // ����� �� ����
    if (busy())
        return false;
    // ������������� �����
    this->socket = socket;
    // �������� ��������� �����
    sockaddr_in addr;
    auto len = (socklen_t)sizeof(addr);
    lwip_getpeername(socket, (struct sockaddr*)&addr, &len);
    lwip_ip2string(addr.sin_addr, address);
    sprintf(address + strlen(address), ":%d", ntohs(addr.sin_port));
    // ����� � ���
    log("Connected", socket);
    // ��������� ���������� �������� � 500 ��
    timeout_change(500);
    // ��������� �����������
    handler_change(initial_allocator.allocate(*this), WEB_SLOT_FREE_REASON_MIGRATION);
    // �������� �����������
    if (handler == NULL)
    {
        log("Unable to allocate initial handler slot!");
        free(WEB_SLOT_FREE_REASON_INSIDE);
    }
    return true;
}

void web_slot_socket_t::handler_change(web_slot_handler_t *new_handler, web_slot_free_reason_t reason)
{
    if (handler != NULL)
        handler->free(reason);
    handler = new_handler;
}

void web_slot_socket_t::close_detect(void)
{
    assert(busy() && closing);
    // ���������, ����� �� �������� ������ � �����
    fd_set fd;
    FD_ZERO(&fd);
    FD_SET(socket, &fd);
    timeval tv = { 0, 0 };
    lwip_select(2, NULL, &fd, NULL, &tv);
    if (FD_ISSET(socket, &fd))
    {
        log("Closed");
        close();
        return;
    }
    // ��������� ���������� ������
    if (check_io(lwip_recv(socket, &tv, sizeof(tv), 0)) >= 0)
        return;

    log("Reset!");
    close();
}

void web_slot_socket_t::close(void)
{
    assert(busy());
    closing = false;
    // �������� ������
    lwip_close(socket);
    socket = LWIP_INVALID_SOCKET;
}

void web_slot_socket_t::free(web_slot_free_reason_t reason)
{
    assert(busy());
    // ����� � ���
    switch (reason)
    {
        case WEB_SLOT_FREE_REASON_INSIDE:
            log("Inside close");
            close();
            break;
        case WEB_SLOT_FREE_REASON_OUTSIDE:
            log("Outside closing...");
            closing = true;
            break;
        case WEB_SLOT_FREE_REASON_NETWORK:
            log("Disconnected!");
            close();
            break;
        default:
            assert(false);
            break;
    }
    // ������������ �����������
    handler_change(NULL, reason);
}

void web_slot_socket_t::execute(web_slot_buffer_t buffer)
{
    if (!busy())
        return;
    if (closing)
    {
        close_detect();
        return;
    }
    // �������� �����������
    handler->execute(buffer);
    // ��������� ��������
    if (timeout.period > 0 && os_tick_get() - timeout.start >= timeout.period)
    {
        log("Timeout!");
        free(WEB_SLOT_FREE_REASON_INSIDE);
    }
}

int32_t web_slot_socket_t::check_io(int32_t fb)
{
    // ���� ��������� �������
    if (fb == 0)
        return -1;
    // ���� ��� ������
    if (fb > 0)
    {
        // io_led_green.flash();
        timeout_reset();
        return fb;
    }
    // ������ ������
    switch (errno)
    {
        // ��� ������, ���������
        case EWOULDBLOCK:
            return 0;
    }
    // ������� ������
    return -1;
}

int32_t web_slot_socket_t::read(web_slot_buffer_t buffer)
{
    assert(buffer != NULL);
    // ������
    auto result = check_io(lwip_recv(socket, buffer, sizeof(web_slot_buffer_t), 0));
    // ���� ��������� �������
    if (result < 0)
        free(WEB_SLOT_FREE_REASON_NETWORK);
    return result;
}

int32_t web_slot_socket_t::write(const web_slot_buffer_t buffer, int size)
{
    assert(size > 0);
    // ������
    auto result = check_io(lwip_send(socket, buffer, size, 0));
    // ���� ��������� �������
    if (result < 0)
        free(WEB_SLOT_FREE_REASON_NETWORK);
    return result;
}
