#ifndef __LOG_H
#define __LOG_H

// Сырой вывод
void log_raw(const char *format, ...);
// Вывод строки
void log_line(const char *format, ...);
// Вывод количества памяти
void log_heap(const char *module);
// Модульный вывод
void log_module(const char *module, const char *format, ...);

#endif // __LOG_H
