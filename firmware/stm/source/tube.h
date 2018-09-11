#ifndef __TUBE_H
#define __TUBE_H

#include "hmi.h"

// Количество цифровых разрядов
#define TUBE_NIXIE_COUNT        HMI_RANK_COUNT
// Количество точечных разделителей
#define TUBE_NEON_COUNT         4

// Цифра для пробела
#define TUBE_NIXIE_DIGIT_SPACE  10

// Инициализация модуля
void tube_init(void);
// Обновление состояния ламп
void tube_refresh(void);

// --- Лампы --- //

// Установка цифры (с точкой)
void tube_nixie_digit_set(hmi_rank_t index, uint8_t digit, bool dot);
// Установка текста
void tube_nixie_text_set(const char text[]); // TODO: а надо ли тут?
// Установка насыщенности
void tube_nixie_sat_set(hmi_sat_t value); // TODO: а надо ли тут?
void tube_nixie_sat_set(hmi_rank_t index, hmi_sat_t value);

// --- Неонки --- //

// Установка насыщенности
void tube_neon_sat_set(hmi_sat_t value); // TODO: а надо ли тут?
void tube_neon_sat_set(hmi_rank_t index, hmi_sat_t value);

// Обработчик сброса напряжения ламп
void tube_interrupt_nixie_selcrst(void);

#endif // __TUBE_H
