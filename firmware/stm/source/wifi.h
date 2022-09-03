﻿#ifndef __WIFI_H
#define __WIFI_H

#include <xwifi.h>

// Инициализация модуля
void wifi_init(void);
// Получает есть ли теоретически интернет
bool wifi_has_internet_get(void);

#endif // __WIFI_H
