#ifndef __STM_H
#define __STM_H

#include <ipc.h>
#include "system.h"

// Процессор входящих пактов для STM
extern ipc_processor_ext_t stm_processor_in;

// Инициализация модуля
void stm_init(void);
// Добавление обработчика событий
void stm_add_event_handler(ipc_event_handler_t &handler);

#endif // __STM_H
