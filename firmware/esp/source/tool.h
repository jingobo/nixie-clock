#ifndef __TOOL_H
#define __TOOL_H

#include "system.h"

// Буфер для конвертирования количествой байт в строку
typedef char tool_bts_buffer[11];

// Конвертирование количествой байт в строку
void tool_byte_to_string(size_t bytes, tool_bts_buffer dest);

#endif // __TOOL_H
