#ifndef __WIFI_H
#define __WIFI_H

#include <os.h>

// Инициализация модуля
void wifi_init(void);
// Ожидание появления сетевого интерфейса
bool wifi_wait(os_tick_t ticks = OS_TICK_MIN);

#endif // __WIFI_H
