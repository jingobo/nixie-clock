#ifndef __SYSTEM_H
#define __SYSTEM_H

// Shared
#include <common.h>
// ESP8266
#include <esp_common.h>

// Размер регистра в батйах
#define REG_SIZE            4

// Установка состояния вывода GPIO
#define GPIO_OUT_SET(pin, state)    GPIO_REG_WRITE(((state) ? GPIO_OUT_W1TS_ADDRESS : GPIO_OUT_W1TC_ADDRESS), (1L << (pin)))
// Включение/Отключение вывода GPIO
#define GPIO_ENA_SET(pin, state)    GPIO_REG_WRITE(((state) ? GPIO_ENABLE_W1TS_ADDRESS : GPIO_ENABLE_W1TC_ADDRESS), (1L << (pin)))

// --- FreeRTOS --- //

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Создание задачи с аргументами по умолчанию (стек максимум по докам)
#define CREATE_TASK(routine, name, arg) xTaskCreate(routine, (const signed char *)(name), 512, arg, 1, NULL)
// Вход/Выход из мьютекса
#define MUTEX_ENTER(m)      xSemaphoreTake(m, portMAX_DELAY)
#define MUTEX_LEAVE(m)      xSemaphoreGive(m)
// Осутствующий макрос для пересчета милисекунд в тики
#define portTICK_PERIOD_MS  ((portTickType )1000 / configTICK_RATE_HZ)

#endif // __SYSTEM_H
