#ifndef __WDT_H
#define __WDT_H

// Инициализация модуля
void wdt_init(void);
// Сброс таймера
void wdt_pulse(void);

#endif // __WDT_H
