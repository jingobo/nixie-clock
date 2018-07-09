#ifndef __ESP_H
#define __ESP_H

#include "typedefs.h"

// Инициализация модуля
void esp_init(void);

// SPI транзакция
void esp_transaction(void);

// Обработчик DMA
void esp_interrupt_dma(void);

#endif // __ESP_H
