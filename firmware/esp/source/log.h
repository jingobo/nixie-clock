#ifndef __LOG_H
#define __LOG_H

#include "system.h"

// Переменная имени модуля для лога
#define LOG_TAG             __module_tag
// Декларирование тэга
#define LOG_TAG_DECL(v)     static const char LOG_TAG[] = (v)
// Логирование
#define LOGE(format, ...)   ESP_LOGE(LOG_TAG, format, ##__VA_ARGS__)
#define LOGW(format, ...)   ESP_LOGW(LOG_TAG, format, ##__VA_ARGS__)
#define LOGI(format, ...)   ESP_LOGI(LOG_TAG, format, ##__VA_ARGS__)
#define LOGD(format, ...)   ESP_LOGD(LOG_TAG, format, ##__VA_ARGS__)
#define LOGV(format, ...)   ESP_LOGV(LOG_TAG, format, ##__VA_ARGS__)
#define LOGH()              log_heap(LOG_TAG);

// Вывод количества памяти
void log_heap(const char *module);

#endif // __LOG_H
