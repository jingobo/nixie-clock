#ifndef __STM_H
#define __STM_H

#include <ipc.h>
#include "system.h"

// Класс процессора входящих пактов для STM
class stm_processor_in_t : public ipc_processor_t
{
public:
    // Обработка пакета
    virtual ipc_processor_status_t packet_process(const ipc_packet_t &packet, const ipc_processor_args_t &args);
};

// Процессор входящих пактов для STM
extern stm_processor_in_t stm_processor_in;

// Инициализация модуля
void stm_init(void);
// Добавление обработчика событий
void stm_add_event_handler(ipc_event_handler_t &handler);

#endif // __STM_H
