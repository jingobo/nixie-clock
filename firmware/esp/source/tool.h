#ifndef __TOOL_H
#define __TOOL_H

#include "system.h"

// ����� ��� ��������������� ����������� ���� � ������
typedef char tool_bts_buffer[11];

// ��������������� ����������� ���� � ������
void tool_byte_to_string(size_t bytes, tool_bts_buffer dest);

#endif // __TOOL_H
