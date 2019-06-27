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

// Базовый класс фильтров для неонок
class neon_filter_t : public neon_model_t::filter_t
{
protected:
    // Основой конструктор
    neon_filter_t(hmi_filter_purpose_t purpose) : filter_t(purpose)
    { }
    
    // Смена данных с указанием насыщенности
    void change(hmi_rank_t index, hmi_sat_t sat)
    {
        auto data = data_get(index);
        data.sat = sat;
        data_change(index, data);
    }
    
    // Задает текущее состояния
    void state_set(hmi_rank_t index, bool state)
    {
        change(index, state ? HMI_SAT_MAX : HMI_SAT_MIN);
    }
    
    // Получает текущее состояния
    bool state_get(hmi_rank_t index) const
    {
        return data_get(index).sat > HMI_SAT_MIN;
    }
};

// Инициализация модуля
void neon_init(void);

#endif // __NEON_H
