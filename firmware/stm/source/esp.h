#ifndef __ESP_H
#define __ESP_H

#include <ipc.h>

// Инициализация модуля
void esp_init(void);
// Обработчик DMA
void esp_interrupt_dma(void);
// Добавление обработчика команд
void esp_handler_add(ipc_handler_t &handler);

#endif // __ESP_H
