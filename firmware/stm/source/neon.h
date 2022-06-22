#ifndef __NEON_H
#define __NEON_H

#include "hmi.h"

// Количество неонок
#define NEON_COUNT      4

// Параметры отображения неонки
struct neon_data_t
{
    // Насыщенность
    hmi_sat_t sat;
        
    // Конструктор по умолчанию
    neon_data_t(hmi_sat_t _sat = HMI_SAT_MIN) : 
        sat(_sat)
    { }
    
    // Оператор равенства
    bool operator == (const neon_data_t &other)
    {
        return sat == other.sat;
    }

    // Оператор не равенства
    bool operator != (const neon_data_t &other)
    {
        return sat != other.sat;
    }
    
    // Оператор присваивания
    neon_data_t & operator = (const neon_data_t &source)
    {
        sat = source.sat;
        return *this;
    }
};

// Модель фильтров для неонок
class neon_model_t : public hmi_model_t<neon_data_t, NEON_COUNT>
{ };

// Базовый класс фильтра
class neon_filter_t : public neon_model_t::filter_t
{
protected:
    // Приоритеты фильтров
    enum
    {
        // Освещенность
        PRIORITY_AMBIENT,
        // Плавное изменение
        PRIORITY_SMOOTH,
    };
    
    // Основной конструктор
    neon_filter_t(int8_t priority) : filter_t(priority)
    { }
};

// Инициализация модуля
void neon_init(void);

#endif // __NEON_H
