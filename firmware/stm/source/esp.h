#ifndef __ESP_H
#define __ESP_H

#include <ipc.h>

// Инициализация модуля
void esp_init(void);
// Получает признак активности линии
bool esp_wire_active(void);
// Добавление обработчика команд
void esp_handler_add(ipc_handler_t &handler);

// Обработчик DMA
void esp_interrupt_dma(void);

#endif // __ESP_H
