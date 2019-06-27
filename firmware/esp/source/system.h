#ifndef __SYSTEM_H
#define __SYSTEM_H

// Shared
#include <common.h>
// ESP8266
#include <esp_log.h>
#include <esp_system.h>
#include <esp8266/esp8266.h>
#include <esp8266/eagle_soc.h>

// Размер регистра в батйах
#define SYSTEM_REG_SIZE             4
// Барьер компилятора
#define SYSTEM_COMPILER_BARRIER()   asm volatile("" ::: "memory")

#endif // __SYSTEM_H
