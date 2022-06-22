#ifndef __NIXIE_H
#define __NIXIE_H

#include "hmi.h"

// Количество ламп
#define NIXIE_COUNT             HMI_RANK_COUNT
// Цифра для пробела
#define NIXIE_DIGIT_SPACE       10

// Параметры отображения лампы
struct nixie_data_t
{
    // Выводимая цифра
    uint8_t digit;
    // Насыщенность
    hmi_sat_t sat;
    // Вывод точки
    bool dot;
        
    // Конструктор по умолчанию
    nixie_data_t(hmi_sat_t _sat = HMI_SAT_MIN, uint8_t _digit = NIXIE_DIGIT_SPACE, bool _dot = false) : 
        sat(_sat), digit(_digit), dot(_dot)
    { }
    
    // Оператор равенства
    bool operator == (const nixie_data_t &other)
    {
        return digit == other.digit &&
               sat == other.sat &&
               dot == other.dot;
    }

    // Оператор не равенства
    bool operator != (const nixie_data_t &other)
    {
        return !(*this == other);
    }
    
    // Оператор присваивания
    nixie_data_t & operator = (const nixie_data_t &source)
    {
        digit = source.digit;
        sat = source.sat;
        dot = source.dot;
        return *this;
    }
};

// Класс модели фильтров для ламп
class nixie_model_t : public hmi_model_t<nixie_data_t, NIXIE_COUNT>
{ };

// Базовый класс фильтра
class nixie_filter_t : public nixie_model_t::filter_t
{
protected:
    // Приоритеты фильтров
    enum
    {
        // Освещенность
        PRIORITY_AMBIENT,
        // Плавное изменение насыщенности
        PRIORITY_SMOOTH_SAT,
        // Плавное изменение значения
        PRIORITY_SMOOTH_VAL,
    };
    
    // Основной конструктор
    nixie_filter_t(int8_t priority) : filter_t(priority)
    { }
};

// Развязочная таблица, указаны цифры по убыванию в глубь лампы
extern uint8_t NIXIE_DIGITS_BY_PLACES[10];
// Развязочная таблица, указаны места по возрастанию цифры
extern uint8_t NIXIE_PLACES_BY_DIGITS[10];

// Инициализация модуля
void nixie_init(void);

#endif // __NIXIE_H
