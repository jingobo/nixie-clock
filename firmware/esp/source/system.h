#ifndef __SYSTEM_H
#define __SYSTEM_H

// Shared
#include <common.h>
// ESP8266
#include <esp_common.h>

// Размер регистра в батйах
#define REG_SIZE                    4

// Установка состояния вывода GPIO
#define GPIO_OUT_SET(pin, state)    GPIO_REG_WRITE(((state) ? GPIO_OUT_W1TS_ADDRESS : GPIO_OUT_W1TC_ADDRESS), (1L << (pin)))
// Включение/Отключение вывода GPIO
#define GPIO_ENA_SET(pin, state)    GPIO_REG_WRITE(((state) ? GPIO_ENABLE_W1TS_ADDRESS : GPIO_ENABLE_W1TC_ADDRESS), (1L << (pin)))

// Оповещение о изменении частоты ядра (отсутствует в SDK)
C_SYMBOL void ets_update_cpu_frequency(int freqmhz);

// --- FreeRTOS --- //

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Осутствующий макрос для пересчета милисекунд в тики
#define portTICK_PERIOD_MS          ((portTickType )1000 / configTICK_RATE_HZ)

// Приоритеты задач
#define TASK_PRIORITY_IDLE          0
#define TASK_PRIORITY_NORMAL        1
#define TASK_PRIORITY_CRITICAL      4

// Создание задачи с аргументами по умолчанию (стек максимум по докам)
#define TASK_CREATE(routine, n, a)  xTaskCreate(routine, (const signed char *)(n), 512, a, TASK_PRIORITY_NORMAL, NULL)
// Задает приоритет текущей задачи
#define TASK_PRIORITY_SET(pri)      vTaskPrioritySet(NULL, pri)

// Создание/Вход/Выход из мьютекса (используются рекурсивные мьютексы)
#define MUTEX_CREATE()              xSemaphoreCreateRecursiveMutex()
#define MUTEX_ENTER(m)              xSemaphoreTakeRecursive(m, portMAX_DELAY)
#define MUTEX_LEAVE(m)              xSemaphoreGiveRecursive(m)

#endif // __SYSTEM_H
