#ifndef __HMI_H
#define __HMI_H

#include "typedefs.h"
#include "list.h"

// Количество разрядов дисплея
#define HMI_RANK_COUNT          6
// Кадров в секунду (Гц)
#define HMI_FRAME_RATE          100

// Минимальная насыщенность
#define HMI_SAT_MIN             0
// Максимальная насыщенность
#define HMI_SAT_MAX             UINT8_MAX
// Количество уровней насыщенности
#define HMI_SAT_COUNT           (1U + HMI_SAT_MAX)

// ASCII симаволы
#define HMI_CHAR_ZERO           '0'
#define HMI_CHAR_NINE           '9'
#define HMI_CHAR_SPACE          ' '
#define HMI_CHAR_DOT            '.'

// Тип данных для индексации разряда
typedef uint8_t hmi_rank_t;
// Тип данных для хранения насыщенности компоненты
typedef uint8_t hmi_sat_t;
// Таблица уровней насыщенности
typedef hmi_sat_t hmi_sat_table_t[HMI_SAT_COUNT];

// Тип для хранения цвета в формате RGB
FIELD_ALIGN_ONE
union hmi_rgb_t
{
    uint32_t raw;
    struct
    {
        hmi_sat_t g;
        hmi_sat_t r;
        hmi_sat_t b;
        // Для прозрачности (не используется)
        hmi_sat_t a;
    };
    uint8_t grba[4];
    
    // Конструкторы
    hmi_rgb_t(void) : hmi_rgb_t(0)
    { }
    hmi_rgb_t(uint32_t _raw) : raw(_raw) 
    { }
    hmi_rgb_t(hmi_sat_t _r, hmi_sat_t _g, hmi_sat_t _b) : r(_r), g(_g), b(_b), a(0) 
    { }

    // Операторы
    bool operator == (const hmi_rgb_t& a) const
    { 
        return raw == a.raw; 
    }

    // Применение гамма коррекции
    void gamma(const hmi_sat_table_t &table);
};

// Тип для хранения цвета в формате HSV
FIELD_ALIGN_ONE
union hmi_hsv_t
{
    uint32_t raw;
    struct
    {
        hmi_sat_t h;
        hmi_sat_t s;
        hmi_sat_t v;
        // Для прозрачности (не используется)
        hmi_sat_t a;
    };
    uint8_t hsva[4];
    
    // Конструкторы
    hmi_hsv_t(void) : hmi_hsv_t(0)
    { }
    hmi_hsv_t(uint32_t _raw) : raw(_raw) 
    { }
    hmi_hsv_t(hmi_sat_t _h, hmi_sat_t _s, hmi_sat_t _v) : h(_h), s(_s), v(_v), a(0) 
    { }
    
    // Операторы
    bool operator == (const hmi_hsv_t& a) const
    { 
        return raw == a.raw;
    }
    
    // Конвертирование HSV в RGB
    hmi_rgb_t to_rgb(void) const;
};

// Назначение передатчика данных
enum hmi_filter_purpose_t
{
    // Элемент цеплчки фильтров
    HMI_FILTER_PURPOSE_CHAIN = 0,
    // Первичный контроллер (источник данных)
    HMI_FILTER_PURPOSE_CONTROLLER,
    
    // Плавная коррекция насыщенности
    HMI_FILTER_PURPOSE_SMOOTH_SAT,
    // Плавная коррекция значения
    HMI_FILTER_PURPOSE_SMOOTH_VAL,
    
    // Мгновенная коррекция насыщенности
    HMI_FILTER_PURPOSE_INSTANT_SAT,
    // Мгновенная коррекция значения
    HMI_FILTER_PURPOSE_INSTANT_VAL,
    
    // Коррекция гаммы
    HMI_FILTER_PURPOSE_GAMMA,
    // Конечный дисплей (приёмнк данных)
    HMI_FILTER_PURPOSE_DISPLAY
};


// Передаваемые данные через фильтры
template <typename DATA, typename PARTS>
class hmi_filter_data_t
{
    // 
    DATA data;
    
};


// Базовый класс фильтров
template <typename DATA, hmi_rank_t COUNT>
class hmi_filter_t : list_item_t
{
    // Последние переданные данные
    DATA last[COUNT];
    
    // Проверка индекса
    static void index_check(hmi_rank_t index)
    { assert(index < COUNT); }
protected:    
    // События смены данных
    virtual void do_data_set(hmi_rank_t index, DATA &data)
    { }
    // События обновления
    virtual void do_refresh(void)
    { }
    
    // Получает приведенный указательн на следующий фильтр
    hmi_filter_t<DATA, COUNT> * next_cast(void) const
    {
        return static_cast<hmi_filter_t<DATA, COUNT> *>(next_get());
    }
    // Безопасная передача данных следующему фильтру
    void next_data_set(hmi_rank_t index, DATA data) const
    {
        auto next = next_cast();
        if (next != NULL)
            next->data_set(index, data);
    }
public:
    // Получает назначение
    const hmi_filter_purpose_t purpose;
    
    // Основой конструктор
    hmi_filter_t(hmi_filter_purpose_t _purpose) : purpose(_purpose)
    { }
    
    // Получает текущие данные
    DATA data_get(hmi_rank_t index) const
    { 
        index_check(index);
        return last[index]; 
    }
    // Устанавливает новые дныые
    void data_set(hmi_rank_t index, DATA data)
    {
        index_check(index);
        // Изменились ли данные
        if (last[index] == data)
            return;
        last[index] = data;
        // Вызов обработчика смены данных
        do_data_set(index, data);
        // Сохраняем текущее значение и переход к следующему фильтру
        next_data_set(index, data);
    }
    // Обнвление данных (тактирование)
    void refresh(void)
    {
        do_refresh();
        // Вызов метода обновления у следующего фильтра
        auto next = next_cast();
        if (next != NULL)
            next->refresh();        
    }
    // Добавление фильтра в цепочку
    void attach(hmi_filter_t &item)
    {
        // Проверка аргументов
        assert(item.purpose > purpose);
        // Добавление
        auto next = next_cast();
        if (next == NULL)
        {
            push(&item);
            return;
        }
        // Не можем вместить между следущим и текущим элементом...
        if (next->purpose < item.purpose)
        {
            next->attach(item);
            return;
        }
        // Вмещаем между текущим и следующим
        push(&item);
        item.push(next);
    }
    // Удаление фильтра из цепочки
    bool detach(hmi_filter_t &item)
    {
        // Проверка аргументов
        assert(&item != this);
        // Исключение
        auto next = next_cast();
        if (next == NULL)
            return false;
        // Нашли фильтр
        if (next == &item)
        {
            item.remove(this);
            return true;
        }
        return next->detach(item);
    }
};

// Цепочка фильтров
template <typename DATA, hmi_rank_t COUNT>
struct hmi_filter_chain_t : hmi_filter_t<DATA, COUNT>
{
    // Конструктор по умолчанию
    hmi_filter_chain_t(void) : hmi_filter_t<DATA, COUNT>(HMI_FILTER_PURPOSE_CHAIN)
    { }
};

// Заполнение таблицы гамма коррекции
void hmi_gamma_prepare(hmi_sat_table_t &table, float_t gamma = 0.45f);
// Применение гамма коррекции
hmi_sat_t hmi_gamma_apply(const hmi_sat_table_t &table, hmi_sat_t original);

// Базовый фильтр гаммы
template <typename DATA, hmi_rank_t COUNT>
class hmi_filter_gamma_t : public hmi_filter_t<DATA, COUNT>
{
    // Текущее значение коофициента гаммы
    float_t gamma;
protected:
    // Таблица коррекции гаммы
    hmi_sat_table_t table;
public:
    // Конструктор по умолчанию
    hmi_filter_gamma_t(void) : hmi_filter_t<DATA, COUNT>(HMI_FILTER_PURPOSE_GAMMA), gamma(0.0f)
    { }
    
    // Устанавливает новое значение коррекции
    void gamma_set(float_t value = 0.45f)
    {
        // Проверка аргументов
        assert(value > 0.0f);
        // Проверка на необходимость пересчета
        if (gamma == value)
            return;
        gamma = value;
        hmi_gamma_prepare(table, gamma);
    }
    // Получает текущее значение коррекции
    float_t gamma_get(void)
    {
        return gamma;
    }
};

#endif // __HMI_H
