#ifndef __DEBUG_H
#define __DEBUG_H

#include "typedefs.h"

// Инициализация модуля
void debug_init();

// Импульс на выводе для отладки
void debug_pulse(void);

// Запись в отладочный вывод
void debug_write(const void *source, size_t size);

// Обработчик DMA
void debug_interrupt_dma(void);

#endif // __DEBUG_H
