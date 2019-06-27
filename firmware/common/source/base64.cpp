#include "base64.h"

// Таблица для кодирования
static const char BASE64_BASIS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(char *encoded, const char *source, size_t len)
{
    // Проверка аргументов
    assert(encoded != NULL && source != NULL);
    size_t i = 0;
    // Обработка входных данных
    for (; i < len - 2; i += 3)
    {
        *encoded++ = BASE64_BASIS[(source[i] >> 2) & 0x3F];
        *encoded++ = BASE64_BASIS[((source[i] & 0x3) << 4) | ((source[i + 1] & 0xF0) >> 4)];
        *encoded++ = BASE64_BASIS[((source[i + 1] & 0xF) << 2) | ((source[i + 2] & 0xC0) >> 6)];
        *encoded++ = BASE64_BASIS[source[i + 2] & 0x3F];
    }
    // Пады
    if (i < len) 
    {
        *encoded++ = BASE64_BASIS[(source[i] >> 2) & 0x3F];
        if (i == (len - 1)) \
        {
            *encoded++ = BASE64_BASIS[((source[i] & 0x3) << 4)];
            *encoded++ = '=';
        }
        else 
        {
            *encoded++ = BASE64_BASIS[((source[i] & 0x3) << 4) | ((source[i + 1] & 0xF0) >> 4)];
            *encoded++ = BASE64_BASIS[((source[i + 1] & 0xF) << 2)];
        }
        *encoded++ = '=';
    }
    *encoded++ = '\0';
}
