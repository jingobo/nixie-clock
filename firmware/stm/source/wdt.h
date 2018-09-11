#ifndef __WDT_H
#define __WDT_H

#include "typedefs.h"

// Инициализация модуля
void wdt_init(void);
// Сброс таймера
void wdt_pulse(void);
// Остановка таймера
void wdt_stop(void);

#endif // __WDT_H
