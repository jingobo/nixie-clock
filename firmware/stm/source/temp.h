#ifndef __TEMP_H
#define __TEMP_H

#include "typedefs.h"

// Инициализация модуля
void temp_init(void);
// Получает текущее покзаание температуры
float_t temp_current_get(void);

// Обработчик прерывания от DMA
void temp_interrupt_dma(void);

#endif // __TEMP_H
