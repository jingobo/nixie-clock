#ifndef __NEON_H
#define __NEON_H

#include "hmi.h"
#include "xmath.h"

// Количество неонок
constexpr const hmi_rank_t NEON_COUNT = 4;

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
    
    // Получает среднюю яркость к указанноой в соотношении
    neon_data_t smooth(neon_data_t to, uint32_t ratio, uint32_t ratio_max) const
    {
        to.sat = math_value_ratio(sat, to.sat, ratio, ratio_max);
        return to;
    }
};

// Модель фильтров неонок
using neon_model_t = hmi_model_t<neon_data_t, NEON_COUNT>;

// Класс источника неонок по умолчанию
class neon_source_t : public neon_model_t::source_t
{
public:
    // Тип данных для маски состояния разрядов
    using rank_mask_t = uint8_t;
    
    // Маска состояний со всеми отключенным разрядами
    static constexpr const rank_mask_t RANK_MASK_NONE = 0;
    // Маска состояний со всеми включенными разрядами
    static constexpr const rank_mask_t RANK_MASK_ALL = (1 << NEON_COUNT) - 1;
    
    // Структура настроек
    struct settings_t
    {
        // Маска состояния
        rank_mask_t mask;
        // Период
        hmi_time_t period;
        // Плавность
        hmi_time_t smooth;
        // Инверсия
        bool inversion;
    };
    
    STATIC_ASSERT(sizeof(settings_t) == 4);
    
private:
    // Фаза вывода
    uint8_t phase = 1;
    // Фрейм периода
    uint32_t frame = 0;
    // Фреймов на период
    uint32_t frame_count = 0;
    // Фреймов для синхронизации
    uint32_t frame_count_sync = 0;
    // Используемые настройки
    const settings_t &settings;
    // Контроллер плавного изменения
    neon_model_t::smoother_to_t smoother;
    
    // Получает данные к отображению по состоянию
    neon_data_t data_normal(bool state) const
    {
        return neon_data_t(state ? HMI_SAT_MAX : HMI_SAT_MIN);
    }

    // Запуск эффекта плавности на разряде
    void start_rank_smooth(hmi_rank_t index, neon_data_t to)
    {
        smoother.start(index, out_get(index), to);
    }
    
    // Получает младший бит маски в виде булевы
    static constexpr bool rank_mask_lsb(rank_mask_t mask)
    {
        return (mask & 0x01) != 0;
    }
    
    // Установка начальных данных разрядов
    void set_initial_ranks(void)
    {
        uint8_t mask = settings.mask;
        for (hmi_rank_t i = 0; i < NEON_COUNT; i++, mask >>= 1)
            out_set(i, data_normal(rank_mask_lsb(mask)));
    }
    
protected:
    // Обработчик события присоединения к цепочке
    virtual void attached(void) override final
    {
        // Базовый метод
        source_t::attached();
        
        // Начальные цвета
        set_initial_ranks();
    }

    // Обнвление данных
    virtual void refresh(void) override final;
    
public:
    // Конструктор по умолчанию
    neon_source_t(const settings_t &_settings) : settings(_settings)
    { }
    
    // Синхронизация основного фрйма к секунде
    void second(void)
    {
        if (frame_count <= 0)
            return;
        
        frame_count_sync += HMI_FRAME_RATE;
        frame_count_sync %= frame_count;
        frame = frame_count_sync;
    }
    
    // Обработчик применения настроек
    void settings_apply(const settings_t *settings_old);
};

// Инициализация модуля
void neon_init(void);

#endif // __NEON_H
