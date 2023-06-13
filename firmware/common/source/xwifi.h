#ifndef __WIFI_TYPES_H
#define __WIFI_TYPES_H

#include "common.h"

// IPv4 адрес
union wifi_ip_t
{
    // По октетам
    uint8_t o[4];
    
    // Конструктор по умолчанию
    wifi_ip_t(void)
    {
        memory_clear(o, sizeof(o));
    }
    
    // Оператор равенства
    bool operator == (const wifi_ip_t &other)
    {
        return memcmp(o, other.o, sizeof(o)) == 0;
    }
    
    // Оператор не равенства
    bool operator != (const wifi_ip_t &other)
    {
        return !(*this == other);
    }
    
    // Оператор присваивания
    wifi_ip_t & operator = (const wifi_ip_t &source)
    {
        memcpy(o, source.o, sizeof(o));
        return *this;
    }
    
    // Оператор присваивания
    wifi_ip_t & operator = (uint32_t source)
    {
        memcpy(o, &source, sizeof(o));
        return *this;
    }
};

// Перечисление интерфейса
enum wifi_intf_t : uint8_t
{
    // Подключение к точке
    WIFI_INTF_STATION = 0,
    // Создание точки
    WIFI_INTF_SOFTAP,
};

// Количество интерфейсов
constexpr const uint8_t WIFI_INTF_COUNT = WIFI_INTF_SOFTAP + 1;

#endif // __WIFI_TYPES_H
