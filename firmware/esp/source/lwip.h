#ifndef __LWIP_H
#define __LWIP_H

#include "lwip/sockets.h"
#include "lwip/netdb.h"

// Определние для типа сокета
typedef int lwip_socket_t;

// Определение для невалидного сокета
#define LWIP_INVALID_SOCKET             -1
// Количество символов для хранения IP адреса
#define LWIP_IP_ADDRESS_BUFFER_SIZE     16

// Перевод сокета в не блокирующий режим
bool lwip_socket_nbio(lwip_socket_t socket);
// Инициализация соекта по умолчанию
bool lwip_socket_config(lwip_socket_t socket);
// Конвертирование IP адреса в строку
const char * lwip_ip2string(const struct in_addr &addr, char *dest);

#endif // __LWIP_H
