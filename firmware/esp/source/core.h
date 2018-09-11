#ifndef __CORE_H
#define __CORE_H

#include <ipc.h>
#include "system.h"

// Инициализация модуля
void core_init(void);
// Добавление обработчика в список
void core_handler_add_command(ipc_handler_command_t &handler);
// Передача данных (потокобезопасно)
bool core_transmit(ipc_dir_t dir, const ipc_command_data_t &data);

#endif // __CORE_H
