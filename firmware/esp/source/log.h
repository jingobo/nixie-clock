#ifndef __LOG_H
#define __LOG_H

// ����� �����
void log_raw(const char *format, ...);
// ����� ������
void log_line(const char *format, ...);
// ����� ���������� ������
void log_heap(const char *module);
// ��������� �����
void log_module(const char *module, const char *format, ...);

#endif // __LOG_H
