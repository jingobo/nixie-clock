#ifndef __TOOL_H
#define __TOOL_H

#include "system.h"

// Буфер для конвертирования количествой байт в строку
typedef char tool_bts_buffer_t[11];
// Буфер для конвертирования MAC адреса в строку
typedef char tool_mac_buffer_t[11];

// Конвертирование количествой байт в строку
void tool_byte_to_string(size_t bytes, tool_bts_buffer_t dest);
// Конвертирование количествой байт в строку
void tool_mac_to_string(const uint8_t mac[6], tool_mac_buffer_t dest);

#endif // __TOOL_H
