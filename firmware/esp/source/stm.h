#ifndef __STM_H
#define __STM_H

#include <ipc.h>
#include "system.h"

// Процессор входящих пактов для STM
extern ipc_processor_proxy_t stm_processor_in;

// Инициализация модуля
void stm_init(void);
// Ожидание простоя потока связи
void stm_wait_idle(void);

#endif // __STM_H
