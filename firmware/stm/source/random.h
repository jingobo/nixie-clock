#ifndef __RANDOM_H
#define __RANDOM_H

#include "typedefs.h"

// Вонсит шумовой бит
void random_noise_bit(bool bit);

// Получает случайное значение
uint32_t random_get(uint32_t max);

// Получает случайное значение на указанном диапазоне
inline uint32_t random_range_get(uint32_t min, uint32_t max)
{
    assert(min <= max);
    return min + random_get(max - min + 1);
}

#endif // __RANDOM_H
