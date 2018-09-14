#include "log.h"
#include "tool.h"
#include <stdarg.h>

// Обертка классом для авто инициализации мютекса
static struct log_t
{
    // Буфер для вывода
    char buffer[256];
    // Мьютекс для синхронизации
    const xSemaphoreHandle mutex;

    // Конструктор по умолчанию
    log_t(void) : mutex(MUTEX_CREATE())
    {
        assert(mutex != NULL);
    }
} logger;

// Фикс отсутствия функции vsprintf
ROM static void log_printf(const char * format, va_list args)
{
    vsnprintf(logger.buffer, sizeof(logger.buffer), format, args);
    printf(logger.buffer);
}

ROM void log_raw(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    MUTEX_ENTER(logger.mutex);
        log_printf(format, args);
    MUTEX_LEAVE(logger.mutex);
    va_end(args);
}

ROM void log_line(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    MUTEX_ENTER(logger.mutex);
        log_printf(format, args);
        printf("\n");
    MUTEX_LEAVE(logger.mutex);
    va_end(args);
}

ROM void log_module(const char *module, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    MUTEX_ENTER(logger.mutex);
        printf("[%s] ", module);
        log_printf(format, args);
        printf("\n");
    MUTEX_LEAVE(logger.mutex);
    va_end(args);
}

void log_heap(const char *module)
{
    tool_bts_buffer buf;
    tool_byte_to_string(system_get_free_heap_size(), buf);
    log_module(module, "Free heap size %s", buf);
}
