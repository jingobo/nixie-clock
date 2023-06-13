#ifndef __NIXIE_H
#define __NIXIE_H

#include "hmi.h"
#include "xmath.h"

// Количество ламп
constexpr const hmi_rank_t NIXIE_COUNT = HMI_RANK_COUNT;
// Цифра для пробела
constexpr const uint8_t NIXIE_DIGIT_SPACE = 10;

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
    
    // Получает среднюю яркость к указанноой в соотношении
    nixie_data_t smooth(nixie_data_t to, uint32_t ratio, uint32_t ratio_max) const
    {
        to.sat = math_value_ratio(sat, to.sat, ratio, ratio_max);
        return to;
    }
};

// Класс модели фильтров для ламп
using nixie_model_t = hmi_model_t<nixie_data_t, NIXIE_COUNT>;

// Базовый класс фильтра
class nixie_filter_t : public nixie_model_t::filter_t
{
protected:
    // Приоритет фильтра переключателя
    static constexpr const uint8_t PRIORITY_SWITCHER = nixie_model_t::PRIORITY_FILTER_MIN + 1;

    // Основной конструктор
    nixie_filter_t(uint8_t priority) : filter_t(priority)
    { }
};

// Класс фильтра переключателя при смене цифр по умолчанию
class nixie_switcher_t : public nixie_filter_t
{
public:
    // Перечисление эффектов смены цифр
    enum effect_t : uint8_t
    {
        // Отсутствует
        EFFECT_NONE = 0,
        // По умолчанию (исчезание старой, повляение новой)
        EFFECT_SMOOTH_DEF,
        // Совмещение (исчезновения старой и появления новой)
        EFFECT_SMOOTH_SUB,
        
        // По умолчанию (порядок следования цифр)
        EFFECT_SWITCH_DEF,
        // По порядку в глубину лампы (от себя)
        EFFECT_SWITCH_IN,
        // По порядку из глубины лампы (на себя)
        EFFECT_SWITCH_OUT,
    };
    
    // Структура настроек
    struct settings_t
    {
        // Эффект смены
        effect_t effect;
    };
    
    STATIC_ASSERT(sizeof(settings_t) == 1);

private:
    // Используемые настройки
    const settings_t &settings;

    // Данные для эффекта
    struct effect_data_t
    {
        // На какую цифры изменение
        uint8_t digit_from = NIXIE_DIGIT_SPACE;
        // С какой цифры изменение
        uint8_t digit_to = NIXIE_DIGIT_SPACE;
        // Номер фрейма эффекта
        uint8_t frame;
                
        // Старт эффекта
        void start(uint8_t from, uint8_t to)
        {
            digit_from = from;
            digit_to = to;
            frame = 0;
        }
        
        // Стоп эффекта
        void stop(void)
        {
            digit_from = digit_to;
        }
        
        // Получает, нективен ли эффект
        bool inactive(void) const
        {
            return digit_to == digit_from;
        }
    } effects[NIXIE_COUNT];
        
    // Обновление цифр с плавным эффектом по умолчанию
    void refresh_smooth_default(nixie_data_t &data, effect_data_t &effect);
    
    // Обновление цифр с плавным эффектом замещения
    void refresh_smooth_substitution(nixie_data_t &data, effect_data_t &effect);
    
    // Обнволение цифр с эффектом переключения
    void refresh_switch(nixie_data_t &data, effect_data_t &effect);

    // Получает признак отсутствия эффекта
    bool is_effect_empty(void) const
    {
        return settings.effect == EFFECT_NONE;
    }
protected:
    // Событие присоединения к цепочке
    virtual void attached(void) override final
    {
        // Базовый метод
        nixie_filter_t::attached();
        
        // Стоп эффекта по всем разрядам
        for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
            effects[i].stop();
    }
    
    // Обнвление данных
    virtual void refresh(void) override final;
    
    // Событие обработки новых данных
    virtual void data_changed(hmi_rank_t index, nixie_data_t &data) override final;
    
public:
    // Конструктор по умолчанию
    nixie_switcher_t(const settings_t &_settings) : 
        nixie_filter_t(nixie_filter_t::PRIORITY_SWITCHER), 
        settings(_settings)
    { }
    
};

// Инициализация модуля
void nixie_init(void);

#endif // __NIXIE_H
