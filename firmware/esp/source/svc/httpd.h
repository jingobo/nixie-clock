#ifndef __HTTPD_H
#define __HTTPD_H

#include <ipc.h>

// Класс процессора входящих пактов для Web
class httpd_processor_in_t : public ipc_processor_t
{
public:
    // Обработка пакета
    virtual ipc_processor_status_t packet_process(const ipc_packet_t &packet, const ipc_processor_args_t &args);
};

// Процессор входящих пактов для Web
extern httpd_processor_in_t httpd_processor_in;

// Инициализация веб сервера
void httpd_init(void);

#endif // __HTTPD_H
