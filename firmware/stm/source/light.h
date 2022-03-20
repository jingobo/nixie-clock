#ifndef __LIGHT_H
#define __LIGHT_H

#include "sensor.h"

// Инициализация модуля
void light_init(void);
// Получает текущее покзаание освещения в люксах
sensor_value_t light_current_get(void);

#endif // __LIGHT_H
