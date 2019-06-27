#include "tool.h"
#include "log.h"

// Имя модуля для логирования
LOG_TAG_DECL("TOOL");

void tool_byte_to_string(size_t bytes, tool_bts_buffer_t dest)
{
    // Проверка аргументов
    assert(dest != NULL);
    // Локальные константы
    const size_t MUL = 1024;
    const size_t KB = MUL;
    const size_t MB = MUL * KB;
    const size_t GB = MUL * MB;
    // Определение делителя и суффикса
    size_t divider = 1;
    const char *suffix = "B";
    do
    {
        if (bytes >= GB)
        {
            suffix = "GB";
            divider = GB;
            break;
        }
        if (bytes >= MB)
        {
            suffix = "MB";
            divider = MB;
            break;
        }
        if (bytes >= KB)
        {
            suffix = "KB";
            divider = KB;
        }
    } while (false);
    // Целая и дробная часть
    auto i = bytes / divider;
    auto f = bytes - i * divider;
    // Перевод в [0..99]
    f *= 100;
    f /= MUL;
    // Вывод
    sprintf(dest, "%d.%02d %s (%d B)", i, f, suffix, bytes);
}

void tool_mac_to_string(const uint8_t mac[6], tool_mac_buffer_t dest)
{
    // Проверка аргументов
    assert(mac != NULL);
    assert(dest != NULL);
    // Вывод
    sprintf(dest, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);
}
