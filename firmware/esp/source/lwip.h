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

// Инициализация соекта в блокирующий режим
bool lwip_socket_bio(lwip_socket_t socket);
// Перевод сокета в не блокирующий режим
bool lwip_socket_nbio(lwip_socket_t socket);
// Включение опции SO_LINGER у указанного соекта
bool lwip_socket_linger(lwip_socket_t socket);
// Конвертирование IP адреса в строку
const char * lwip_ip2string(const struct in_addr &addr, char *dest);

#endif // __LWIP_H
