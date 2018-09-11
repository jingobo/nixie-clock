#ifndef __ESP_H
#define __ESP_H

#include <ipc.h>

// ������������� ������
void esp_init(void);
// ���������� DMA
void esp_interrupt_dma(void);

// ���������� ����������� �������
void esp_handler_add_idle(ipc_handler_idle_t &handler);
// ���������� ����������� ������
void esp_handler_add_command(ipc_handler_command_t &handler);
// �������� ������
bool esp_transmit(ipc_dir_t dir, const ipc_command_data_t &data);

#endif // __ESP_H
