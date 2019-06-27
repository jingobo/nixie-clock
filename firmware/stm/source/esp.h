#ifndef __ESP_H
#define __ESP_H

#include <ipc.h>

// Инициализация модуля
void esp_init(void);
// Обработчик DMA
void esp_interrupt_dma(void);

// Передача данных
bool esp_transmit(ipc_dir_t dir, ipc_command_t &command);
// Добавление обработчика событий
void esp_add_event_handler(ipc_event_handler_t &handler);
// Добавление обработчика команд
void esp_add_command_handler(ipc_command_handler_t &handler);

#endif // __ESP_H
