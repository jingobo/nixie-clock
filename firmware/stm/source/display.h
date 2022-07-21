#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "wifi.h"

// Инициализация модуля
void display_init(void);
// Запрос показа IP адреса
void display_show_ip(const wifi_intf_t &intf, wifi_ip_t ip);

#endif // __DISPLAY_H
