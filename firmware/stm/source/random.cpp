#include "random.h"

// Используемый полином LFSR
constexpr const uint32_t RANDOM_POLY = 0x80200003;

// Начальное значение LFSR регистра
static uint32_t random_lfsr = RANDOM_POLY;
// Шумовое значение регистра
static uint32_t random_noise = 0;
// Количество первых пропускаемых значений
static uint8_t random_skip = 10;

// Обработка следущего значения LFSR
static void random_lfsr_next(void)
{
    const bool lsb = (random_lfsr & 1) != 0;
    random_lfsr >>= 1;
    if (lsb)
        random_lfsr ^= RANDOM_POLY;
}

uint32_t random_get(uint32_t max)
{
    // Пропуск первых значений
    for (; random_skip > 0; random_skip--)
        random_lfsr_next();
    
    // Расчет следущего с наложением шума
    random_lfsr_next();
    return (random_lfsr ^ random_noise) % max;
}

void random_noise_bit(bool bit)
{
    random_noise <<= 1;
    random_noise |= bit ? 1 : 0;
}
