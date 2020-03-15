#ifndef __TEMP_H
#define __TEMP_H

#include "typedefs.h"

// Значение температуры как "нет данных"
#define TEMP_VALUE_EMPTY        999.0f

// Инициализация модуля
void temp_init(void);
// Запуск измерения температуры
void temp_measure(void);
// Получает текущее покзаание температуры
float32_t temp_current_get(void);

// Обработчик прерывания от DMA
void temp_interrupt_dma(void);

#endif // __TEMP_H
