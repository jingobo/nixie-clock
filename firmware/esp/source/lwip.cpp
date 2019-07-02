#include "system.h"
#include "lwip.h"
#include "log.h"

// Имя модуля для логирования
LOG_TAG_DECL("LWIP");

bool lwip_socket_bio(lwip_socket_t socket)
{
    // Проверка аргументов
    assert(socket > LWIP_INVALID_SOCKET);
    // Установка таймаута ожидания данных в 100 мС
#if LWIP_SO_SNDRCVTIMEO_NONSTANDARD
    auto tv = 100;
#else
    timeval tv = { 0, 100000 };
#endif
    auto result = lwip_setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (result != 0)
    {
        LOGE("Unable to set read timeout: %d", result);
        return false;
    }
    return true;
}

bool lwip_socket_linger(lwip_socket_t socket)
{
    // Проверка аргументов
    assert(socket > LWIP_INVALID_SOCKET);
    // Установка опции в 1 секунду
    linger lg = { 1, 1 };
    auto result = lwip_setsockopt(socket, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
    if (result != 0)
    {
        LOGE("Unable to set linger timeout: %d", result);
        return false;
    }
    return true;
}

bool lwip_socket_nbio(lwip_socket_t socket)
{
    // Проверка аргументов
    assert(socket > LWIP_INVALID_SOCKET);
    // Установка опции
    long opt = 1;
    auto result = lwip_ioctl(socket, FIONBIO, &opt);
    if (result < 0)
    {
        LOGE("Unable switch to non-blocking mode: %d", result);
        return false;
    }
    return true;
}

const char * lwip_ip2string(const struct in_addr &addr, char *dest)
{
    // Проверка аргументов
    assert(dest != NULL);
    // Конвертирование
    return inet_ntoa_r(addr, dest, LWIP_IP_ADDRESS_BUFFER_SIZE);
}
