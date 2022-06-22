#ifndef __LIGHT_H
#define __LIGHT_H

#include "typedefs.h"

// Тип уровня освещенности
typedef uint8_t light_level_t;

// Максимальное значение уровня освещенности
constexpr const light_level_t LIGHT_LEVEL_MAX = 100;

// Инициализация модуля
void light_init(void);
// Получает текущий уровень освещенности
light_level_t light_level_get(void);

#endif // __LIGHT_H
