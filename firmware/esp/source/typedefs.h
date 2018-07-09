#ifndef __TYPEDEFS_H
#define __TYPEDEFS_H

// --- ESP8266 --- //

#include <esp_common.h>

#define ICACHE_FLASH_ATTR 			__attribute__((section(".irom0.text")))
#define ICACHE_RAM_ATTR 			__attribute__((section(".iram0.text")))

// Оборачивание кода макроса в блок
#define SAFE_BLOCK(code)			\
	do								\
	{								\
		code;						\
	} while (false)

// Установка состояния вывода GPIO
#define GPIO_OUT_SET(pin, state) 	GPIO_REG_WRITE(((state) ? GPIO_OUT_W1TS_ADDRESS : GPIO_OUT_W1TC_ADDRESS), (1L << (pin)))
// Включение/Отключение вывода GPIO
#define GPIO_ENA_SET(pin, state) 	GPIO_REG_WRITE(((state) ? GPIO_ENABLE_W1TS_ADDRESS : GPIO_ENABLE_W1TC_ADDRESS), (1L << (pin)))

// --- FreeRTOS --- //

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Создание задачи с аргументами по умолчанию
#define CREATE_TASK(routine, name, arg)		xTaskCreate(routine, name, configMINIMAL_STACK_SIZE, arg, 1, NULL)

#endif // __TYPEDEFS_H
