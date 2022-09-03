#ifndef __XMATH_H
#define __XMATH_H

#include "typedefs.h"

// Структура 2D точки
template <typename X, typename Y>
struct math_point2d_t
{
    // Координаты точки
    X x;
    Y y;
};

// Функция линейной интерполяции
template <typename X, typename Y>
inline Y math_linear_interpolation(X input_x, const math_point2d_t<X, Y> *points, size_t point_count)
{
    assert(point_count > 0);
    
    // Границы таблицы
    const auto TOP = 0;
    const auto BOTTOM = point_count - 1;
    
    // Определение смещения
    if (input_x <= points[TOP].x)
        return points[TOP].y;
    
    if (input_x >= points[BOTTOM].x)
        return points[BOTTOM].y;
    
    // Поиск соседних точек
    Y result;
    for (int8_t i = BOTTOM; i >= TOP; i--)
        if (input_x >= points[i].x)
        {
            auto j = i + 1;
            // Линейная апроксимация
            auto y = points[i].y;
            auto dy = points[j].y - y;
            
            auto x = points[i].x;
            auto dx = points[j].x - x;
            
            auto k = (input_x - x) / dx;
            result = (Y)(y + dy * k);
            break;
        }
    
    return result;
}

// Функция приведения числа из from в to в соответствии с отношением
template <typename VALUE>
inline VALUE math_value_ratio(VALUE value_from, VALUE value_to, uint32_t ratio, uint32_t ratio_max)
{
    assert(ratio_max > 0);
    
    if (value_from > value_to)
        return (VALUE)(value_from - (uint32_t)((value_from - value_to) * ratio) / ratio_max);
    
    return (VALUE)(value_from + (uint32_t)((value_to - value_from) * ratio) / ratio_max);
}

#endif // __XMATH_H
