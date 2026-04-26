#ifndef __LED_H
#define __LED_H

#include "hmi.h"
#include "xmath.h"

// Количество светодиодов
constexpr const hmi_rank_t LED_COUNT = HMI_RANK_COUNT;

// Параметры отображения светодиода
struct led_data_t
{
    // Данные цвета
    hmi_rgb_t rgb;
    
    // Конструктор по умолчанию
    led_data_t(void) : rgb(HMI_COLOR_RGB_BLACK)
    { }
    
    // Основной конструктор
    led_data_t(hmi_rgb_t _rgb) : rgb(_rgb)
    { }
    
    // Оператор равенства
    bool operator == (const led_data_t &other)
    {
        return rgb == other.rgb;
    }

    // Оператор не равенства
    bool operator != (const led_data_t &other)
    {
        return rgb != other.rgb;
    }
    
    // Получает средний цвет к указанному в соотношении
    led_data_t smooth(led_data_t to, uint32_t ratio, uint32_t ratio_max) const
    {
        to.rgb.r = math_value_ratio(rgb.r, to.rgb.r, ratio, ratio_max);
        to.rgb.g = math_value_ratio(rgb.g, to.rgb.g, ratio, ratio_max);
        to.rgb.b = math_value_ratio(rgb.b, to.rgb.b, ratio, ratio_max);
        return to;
    }
};

// Модель фильтров светодиодов
using led_model_t = hmi_model_t<led_data_t, LED_COUNT>;

// Источник данных подсветки по умолчанию
class led_source_t : public led_model_t::source_smoother_t
{
public:
    // Перечисление эффектов
    enum effect_t : uint8_t
    {
        // Отсутствует
        EFFECT_NONE = 0,
        // Заливка
        EFFECT_FILL,
        // Вспышка
        EFFECT_FUSION,
        // Вспышка 2
        EFFECT_FLASH,
        // Градиент
        EFFECT_GRAD,
        // Разные разряды
        EFFECT_RANKS,
        // Зебра
        EFFECT_ZEBRA,
        // Влево
        EFFECT_LEFT,
        // Вправо
        EFFECT_RIGHT,
        // К центру
        EFFECT_IN,
        // От центра
        EFFECT_OUT,
        // Случайный
        EFFECT_RAND
    };

    // Перечисление источника данных
    enum data_source_t : uint8_t
    {
        // Указаннеые цвета последовательно
        DATA_SOURCE_CUR_SEQ = 0,
        // Указаннеые цвета случайно
        DATA_SOURCE_CUR_RANDOM,
        // Любые случайные цвета
        DATA_SOURCE_ANY_RANDOM,
    };
    
    // Структура настроек
    struct settings_t
    {
        // Эффект смены
        effect_t effect;
        // Задержка
        hmi_time_t delay;
        // Плавность
        hmi_time_t smooth : 6;
        // Источник данных
        data_source_t source : 2;
        // Статичные цвета
        hmi_rgb_t rgb[LED_COUNT];
    };
    
    STATIC_ASSERT(sizeof(settings_t) == 21);

private:
    // Половина количества разрядов
    static constexpr const hmi_rank_t LED_COUNT_HALF = LED_COUNT / 2;
    
    // Используемые настройки
    const settings_t &settings;
    
    // Предыдущий выбранный оттенок
    hmi_sat_t hue_last = 0;
    // Предыдущий выбранный разряд
    hmi_rank_t rank_last = 0;
    // Предыдущий выбранный цвет
    hmi_rgb_t color_last = HMI_COLOR_RGB_BLACK;
    // Предыдущий выбранный разряд цвета
    hmi_rank_t rank_last_color = 0;
    // Таймаут предварительного просмотра в секундах
    uint8_t rgb_preview_timeout = 0;
    
    // Текущий случайный эффект
    effect_t rand_effect = EFFECT_NONE;
    // Текущая фаза случайного эффекта
    uint8_t rand_effect_phase = 0;
    // Количество фаз случайного эффекта
    uint8_t rand_effect_phase_count = 0;
    
    // Текущий кадр задержки
    uint32_t delay_frame = 0;
    // Количество кадров задержки
    uint32_t delay_frame_count = 0;
    
    // Получает текущий эффект
    effect_t effect_get() const
    {
        if (settings.effect != EFFECT_RAND)
            return settings.effect;
        
        assert(rand_effect != EFFECT_RAND);
        return rand_effect;
    }
    
    // Получает следующий случайный эффект
    void rand_effect_next();
    
    // Получает признак активности предварительного просмотра цветов
    bool is_rgb_preview() const
    {
        return rgb_preview_timeout > 0;
    }
    
    // Сравнение RGB данных
    static bool rgb_equals(const hmi_rgb_t *a, const hmi_rgb_t *b)
    {
        for (auto i = 0; i < LED_COUNT; i++)
            if (a[i] != b[i])
                return false;
        
        return true;
    }
    
    // Обработка разряда эффекта вспышки
    void process_rank_flash(hmi_rank_t index)
    {
        // Там может быть переполнение
        if (index < LED_COUNT)
            smoother_start(index, led_data_t(color_last).smooth(out_get(index), 80, 100));
    }

    // Обработка разряда эффекта вспышки 2
    void process_rank_flash2(hmi_rank_t index, const hmi_rgb_t& to)
    {
        // Там может быть переполнение
        if (index < LED_COUNT)
            smoother_start(index, led_data_t(color_last).smooth(to, 50, 100));
    }
    
    // Обработка разряда эффекта по умолчанию
    void process_rank_default(hmi_rank_t index)
    {
        if (index < LED_COUNT)
            smoother_start(index, color_last);
    }
    
    // Формирует случайный цвет
    void color_last_next(void);
    
    // Установка начальных данных разрядов
    void set_initial_ranks(void);
        
protected:
    // Обработчик события присоединения к цепочке
    virtual void attached(void) override final
    {
        // Базовый метод
        source_smoother_t::attached();
        
        // Начальные цвета
        set_initial_ranks();
    }

    // Обновление данных
    virtual void refresh(void) override final;

public:
    // Конструктор по умолчанию
    led_source_t(const settings_t &_settings) : settings(_settings)
    { }
    
    // Обработчик секундного события
    void second(void)
    {
        if (is_rgb_preview())
            if (--rgb_preview_timeout <= 0)
                set_initial_ranks();
    }
    
    // Обработчик применения настроек
    void settings_apply(const settings_t *settings_old);
};

// Получает случайный цвет
hmi_rgb_t led_random_color_get(hmi_sat_t &hue_last, hmi_sat_t value = HMI_SAT_MAX);

// Инициализация модуля
void led_init(void);

// Обработчик DMA
void led_interrupt_dma(void);

#endif // __LED_H
