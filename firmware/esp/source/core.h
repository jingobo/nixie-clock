#ifndef __CORE_H
#define __CORE_H

#include <ipc.h>
#include "system.h"

// ������������� ������
void core_init(void);
// ���������� ����������� � ������
void core_handler_add_command(ipc_handler_command_t &handler);
// �������� ������ (���������������)
bool core_transmit(ipc_dir_t dir, const ipc_command_data_t &data);

#endif // __CORE_H
