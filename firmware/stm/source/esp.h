#ifndef __ESP_H
#define __ESP_H

#include <ipc.h>

// Инициализация модуля
void esp_init(void);
// Обработчик DMA
void esp_interrupt_dma(void);

// Добавление обработчика простоя
void esp_handler_add_idle(ipc_handler_idle_t &handler);
// Добавление обработчика команд
void esp_handler_add_command(ipc_handler_command_t &handler);
// Передача данных
bool esp_transmit(ipc_dir_t dir, const ipc_command_data_t &data);

#endif // __ESP_H
