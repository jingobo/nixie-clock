#include "rtc.h"
#include "timer.h"
#include "xmath.h"
#include "screen.h"
#include "display.h"

// Количество фреймов смены состояния
#define DISPLAY_SMOOTH_FRAME_COUNT          26
// Количество фреймов смены состояния (пополам)
#define DISPLAY_SMOOTH_FRAME_COUNT_HALF     (DISPLAY_SMOOTH_FRAME_COUNT / 2)

// Класс фильтра плавной смены цифр на лампах
class display_nixie_smooth_val_filter_t : public nixie_filter_t
{
    // Данные для эффекта
    struct effect_t
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
    } effect[NIXIE_COUNT];
    
    // Получает следующиую цифру по месту "из глубины"
    static uint8_t next_digit_out_depth(uint8_t digit)
    {
        auto place = NIXIE_PLACES_BY_DIGITS[digit];
        if (place > 0)
            place--;
        else
            place = 9;
        return NIXIE_DIGITS_BY_PLACES[place];
    }
    
    // Получает следующиую цифру по месту "в глубину"
    static uint8_t next_digit_in_depth(uint8_t digit)
    {
        auto place = NIXIE_PLACES_BY_DIGITS[digit];
        if (place < 9)
            place++;
        else
            place = 0;
        return NIXIE_DIGITS_BY_PLACES[place];
    }
    
    // Обновление цифр с плавным эффектом по умолчанию
    void refresh_smooth_default(void)
    {
        // Обход разрядов
        for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
        {
            // Поулчаем ссылку на данные эффекта
            auto &e = effect[i];
            // Если эффект не запущен - выходим
            if (e.inactive())
                continue;
            
            auto data = get(i);
            
            // Фреймы исчезновения старой цифры
            if (e.frame < DISPLAY_SMOOTH_FRAME_COUNT_HALF)
            {
                data.sat = xmath_value_ratio<hmi_sat_t>(data.sat, HMI_SAT_MIN, e.frame, DISPLAY_SMOOTH_FRAME_COUNT_HALF);
                data.digit = e.digit_from;
            }
            // Фреймы появления новой цифры
            else if (e.frame < DISPLAY_SMOOTH_FRAME_COUNT)
                data.sat = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.sat, e.frame - DISPLAY_SMOOTH_FRAME_COUNT_HALF, DISPLAY_SMOOTH_FRAME_COUNT_HALF);
            else
                // Завершение эффекта
                e.stop();
            
            // Передача
            set(i, data);
            e.frame++;
        }
    }
    
    // Обновление цифр с плавным эффектом замещения
    void refresh_smooth_substitution(void)
    {
        // Обход разрядов
        for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
        {
            // Поулчаем ссылку на данные эффекта
            auto &e = effect[i];
            // Если эффект не запущен - выходим
            if (e.inactive())
                continue;
            
            auto data = get(i);
            
            // Фреймы смены цифры
            if (e.frame < DISPLAY_SMOOTH_FRAME_COUNT)
            {
                auto sat = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.sat, e.frame, DISPLAY_SMOOTH_FRAME_COUNT);
                if (e.frame & 1)
                {
                    data.sat = sat;
                    data.digit = e.digit_to;
                }
                else
                    data.sat = data.sat - sat;
            }
            // Фреймы довода насыщенности
            else if (e.frame < DISPLAY_SMOOTH_FRAME_COUNT + 5)
            {
                auto sat = xmath_value_ratio<hmi_sat_t>(180, HMI_SAT_MAX, e.frame - DISPLAY_SMOOTH_FRAME_COUNT, 5);
                data.sat = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, sat, data.sat, HMI_SAT_MAX);
            }
            else
                // Завершение эффекта
                e.stop();
            
            // Передача
            set(i, data);
            e.frame++;
        }
    }
    
    // Обнволение цифр с эффектом переключения
    void refresh_switch(void)
    {
        // Выполнение эффекта
        for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
        {
            // Поулчаем ссылку на данные эффекта
            auto &e = effect[i];
            
            // Если эффект не запущен - выходим
            if (e.inactive())
                continue;
            if (e.frame++ % 4 != 3)
                continue;
            
            uint8_t place;
            switch (kind)
            {
                case KIND_SWITCH_DEFAULT:
                    if (++e.digit_from >= NIXIE_DIGIT_SPACE && e.digit_to != NIXIE_DIGIT_SPACE)
                        e.digit_from = 0;
                    break;
                case KIND_SWITCH_DEPTH_IN:
                    place = NIXIE_PLACES_BY_DIGITS[e.digit_from];
                    if (place != 9 || e.digit_to != NIXIE_DIGIT_SPACE)
                        e.digit_from = next_digit_in_depth(e.digit_from);
                    else
                        e.digit_from = NIXIE_DIGIT_SPACE;
                    break;
                case KIND_SWITCH_DEPTH_OUT:
                    place = NIXIE_PLACES_BY_DIGITS[e.digit_from];
                    if (place != 0 || e.digit_to != NIXIE_DIGIT_SPACE)
                        e.digit_from = next_digit_out_depth(e.digit_from);
                    else
                        e.digit_from = NIXIE_DIGIT_SPACE;
                    break;
                default:
                    assert(false);
                    break;
            }
            
            // Передача
            auto data = get(i);
            data.digit = e.digit_from;
            set(i, data);
        }
    }
protected:
    // Событие присоединения к цепочке
    virtual void attached(void) override final
    {
        // Базовый метод
        nixie_filter_t::refresh();
        // Стоп эффекта по всем разрядам
        for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
            effect[i].stop();
    }
    
    // Обнвление данных
    virtual void refresh(void) override final
    {
        // Базовый метод
        nixie_filter_t::refresh();
        // Выполнение эффекта
        switch (kind)
        {
            case KIND_SMOOTH_DEFAULT:
                refresh_smooth_default();
                break;
            case KIND_SMOOTH_SUBSTITUTION:
                refresh_smooth_substitution();
                break;
            case KIND_SWITCH_DEFAULT:
            case KIND_SWITCH_DEPTH_IN:
            case KIND_SWITCH_DEPTH_OUT:
                refresh_switch();
                break;
            default:
                assert(false);
                break;
        }
    }
    
    // Событие обработки новых данных
    virtual void data_changed(hmi_rank_t index, nixie_data_t &data) override final
    {
        auto &e = effect[index];
        // Проверка, изменилась ли цифра
        auto last = get(index);
        if (last.digit != data.digit)
            do
            {
                // Старт эффекта
                e.start(last.digit, data.digit);
                switch (kind)
                {
                    case KIND_SMOOTH_DEFAULT:
                    case KIND_SMOOTH_SUBSTITUTION:
                        // Ничего
                        continue;
                    case KIND_SWITCH_DEFAULT:
                        // Переход к следующей цифре
                        if (++e.digit_from >= NIXIE_DIGIT_SPACE)
                            e.digit_from = 0;
                        break;
                    case KIND_SWITCH_DEPTH_IN:
                        // Переход к следующему месту
                        if (e.digit_from >= NIXIE_DIGIT_SPACE)
                            e.digit_from = NIXIE_DIGITS_BY_PLACES[0];
                        else
                            e.digit_from = next_digit_in_depth(e.digit_from);
                        break;
                    case KIND_SWITCH_DEPTH_OUT:
                        // Переход к следующему месту
                        if (e.digit_from >= NIXIE_DIGIT_SPACE)
                            e.digit_from = NIXIE_DIGITS_BY_PLACES[9];
                        else
                            e.digit_from = next_digit_out_depth(e.digit_from);
                        break;
                    default:
                        assert(false);
                        break;
                }
                data.digit = e.digit_from;
            } while (false);
        
        // Базовый метод
        nixie_filter_t::data_changed(index, data);
    }
public:
    // Вид перехода
    enum kind_t
    {
        // По умолчанию (исчезание старой, повляение новой)
        KIND_SMOOTH_DEFAULT = 0,
        // Совмещение (исчезновения старой и появления новой)
        KIND_SMOOTH_SUBSTITUTION,
        
        // По умолчанию (порядок следования цифр)
        KIND_SWITCH_DEFAULT,
        // По порядку в глубину лампы (от себя)
        KIND_SWITCH_DEPTH_IN,
        // По порядку из глубины лампы (на себя)
        KIND_SWITCH_DEPTH_OUT,
    } kind = KIND_SMOOTH_DEFAULT;

    // Конструктор по умолчанию
    display_nixie_smooth_val_filter_t(void) : nixie_filter_t(PRIORITY_SMOOTH_VAL)
    { }
};

// Класс фильтра плавной смены состояния неонок
class display_neon_smooth_filter_t : public neon_filter_t
{
    // Данные для эффекта
    struct effect_t 
    {
        // Конечная насыщенность
        hmi_sat_t sat_to = HMI_SAT_MIN;
        // Начальная насыщенность
        hmi_sat_t sat_from = HMI_SAT_MIN;
        // Номер фрейма
        uint8_t frame;
        
        // Старт эффекта
        void start(hmi_sat_t from, hmi_sat_t to)
        {
            // Расчет начальной/конечной насыщенности
            sat_from = from;
            sat_to = to;
            // Сброс номера фрейма
            frame = 0;
        }
                
        // Стоп эффекта
        void stop(void)
        {
            sat_from = sat_to;
        }
        
        // Поулчает, не активен ли эффект
        bool inactive(void) const
        {
            return sat_from == sat_to;
        }
    } effect[NEON_COUNT];
    
protected:
    // Событие присоединения к цепочке
    virtual void attached(void) override final
    {
        // Базовый метод
        neon_filter_t::refresh();
        // Стоп эффекта по всем разрядам
        for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
            effect[i].stop();
    }
    
    // Обнвление данных
    virtual void refresh(void) override final
    {
        // Базовый метод
        neon_filter_t::refresh();
        
        // Выполнение эффекта
        for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
        {
            // Поулчаем ссылку на данные эффекта
            auto &e = effect[i];
            // Если эффект не запущен - выходим
            if (e.inactive())
                continue;
            
            // Смена насыщенности относительно текущего фрейма
            auto data = get(i);
            data.sat = xmath_value_ratio<hmi_sat_t>(e.sat_from, e.sat_to, e.frame, DISPLAY_SMOOTH_FRAME_COUNT);
            set(i, data);
            
            // Инкремент фрейма, и проверка на завершение эффекта
            if (++e.frame > DISPLAY_SMOOTH_FRAME_COUNT)
                e.stop();
        }
    }
    
    // Событие обработки новых данных
    virtual void data_changed(hmi_rank_t index, neon_data_t &data) override final
    {
        auto &e = effect[index];
        // Проверка, изменилась ли насыщенность
        auto last = get(index);
        if (data != last)
        {
            // Старт эффекта
            e.start(last.sat, data.sat);
            // Применяем старые данные
            data = last;
        }
        
        // Базовый метод
        neon_filter_t::data_changed(index, data);
    }
public:
    // Конструктор по умолчанию
    display_neon_smooth_filter_t(void) : neon_filter_t(PRIORITY_SMOOTH)
    { }
};

// Класс фильтра плавной смены цвета светододов
class display_led_smooth_filter_t : public led_filter_t
{
    // Данные для эффекта
    struct effect_t 
    {
        // Конечный цвет
        hmi_rgb_t color_to;
        // Начальный цвет
        hmi_rgb_t color_from;
        // Номер фрейма
        uint32_t frame;
        
        // Старт эффекта
        void start(hmi_rgb_t from, hmi_rgb_t to)
        {
            // Сброс номера фрейма
            frame = 0;
            // Расчет начальной/конечной насыщенности
            color_to = to;
            color_from = from;
        }

        // Стоп эффекта
        void stop(void)
        {
            color_from = color_to;
        }
        
        // Поулчает, не активен ли эффект
        bool inactive(void) const
        {
            return color_from == color_to;
        }
    } effect[LED_COUNT];
    
protected:
    // Событие присоединения к цепочке
    virtual void attached(void) override final
    {
        // Базовый метод
        led_filter_t::attached();
        // Стоп эффекта по всем разрядам
        for (hmi_rank_t i = 0; i < LED_COUNT; i++)
            effect[i].stop();
    }
    
    // Обнвление данных
    virtual void refresh(void) override final
    {
        // Базовый метод
        led_filter_t::refresh();
        
        // Выполнение эффекта
        for (hmi_rank_t i = 0; i < LED_COUNT; i++)
        {
            // Поулчаем ссылку на данные эффекта
            auto &e = effect[i];
            // Если эффект не запущен - выходим
            if (e.inactive())
                continue;
            
            // Расчет текущего цвета по фрейму
            auto data = get(i);
            data.r = xmath_value_ratio<hmi_sat_t>(e.color_from.r, e.color_to.r, e.frame, frame_count);
            data.g = xmath_value_ratio<hmi_sat_t>(e.color_from.g, e.color_to.g, e.frame, frame_count);
            data.b = xmath_value_ratio<hmi_sat_t>(e.color_from.b, e.color_to.b, e.frame, frame_count);
            set(i, data);
            
            // Инкремент фрейма и проверка на завершение эффекта
            if (++e.frame > frame_count)
                e.stop();
        }
    }
    
    // Событие обработки новых данных
    virtual void data_changed(hmi_rank_t index, hmi_rgb_t &data) override final
    {
        auto &e = effect[index];
        // Проверка, изменился ли цвет
        auto last = get(index);
        if (data != last)
        {
            // Старт эффекта
            e.start(last, data);
            // Применяем старый цвет
            data = last;
        }
        
        // Базовый метод
        led_filter_t::data_changed(index, data);
    }
public:
    // Количество фреймов при изменении цвета
    uint32_t frame_count = DISPLAY_SMOOTH_FRAME_COUNT;
    
    // Конструктор по умолчанию
    display_led_smooth_filter_t(void) : led_filter_t(PRIORITY_SMOOTH)
    { }
};

// Класс источника для светодиодов эффекта радуги
class display_led_rainbow_source_t : public led_model_t::source_t
{
    uint16_t time_prescaller, hue;
protected:
    // Обнвление данных
    virtual void refresh(void) override final
    { 
        if (++time_prescaller < HMI_FRAME_RATE * 10)
            return;
        time_prescaller = 0;
        
        hmi_hsv_t hsv(0, HMI_SAT_MAX, HMI_SAT_MAX);
        
        for (hmi_rank_t i = 0; i < LED_COUNT; i++)
        {
            hsv.h = ((hue + i) % 32) * 8;
            set(i, hsv.to_rgb());
        }
        hue++;
    }
public:
    // Конструктор по умолчанию
    display_led_rainbow_source_t(void) : time_prescaller(HMI_FRAME_RATE * 10), hue(0)
    { }
};

// Базовый класс сцены
class display_scene_t : public screen_scene_t
{
    friend void display_scene_set_default(void);
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) = 0;
};

// Установка сцены по умолчанию (предварительное объявление)
static void display_scene_set_default(void);

// Сцена начального теста
static class display_scene_test_t : public display_scene_t
{
    // Источник данных для лмап
    class nixie_source_t : public nixie_model_t::source_t
    {
        // Текущая выводимая цифра
        uint8_t digit;
        // Нужно ли обновлять данные
        bool update_needed;
        
        // Установка текущей цифры
        void digit_set(uint8_t value)
        {
            // Проверка аргументов
            assert(value <= 9);
            // Установка полей
            digit = value;
            update_needed = true;
        }
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_t::attached();
            // Начинаем с нулевой цифры
            digit_set(0);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            source_t::refresh();
            
            // Нужно ли обновлять данные
            if (!update_needed)
                return;
            update_needed = false;
            
            // Установка всех разрядов текущей цифрой
            auto data = nixie_data_t(HMI_SAT_MAX, digit, true);
            for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
                set(i, data);
        }
    public:
        // Конструктор по умолчанию
        nixie_source_t(void)
        { }
        
        // Переход к следующей цифре
        void next(void)
        {
            if (digit < 9)
                digit_set(digit + 1);
        }
    } nixie_source;
    
    // Источник данных для неонок
    class neon_source_t : public neon_model_t::source_t
    {
        // Нужно ли обновлять данные
        bool update_needed : 1;
        // Указывает, можно ли выводить лампы
        bool light_allowed : 1;
        
        // Запрос на обновление данных
        void update(bool light)
        {
            update_needed = true;
            light_allowed = light;
        }
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_t::attached();
            // Сброс состояний
            update(false);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            source_t::refresh();
            
            // Нужно ли обновлять данные
            if (!update_needed)
                return;
            update_needed = false;
            
            // Установка всех разрядов текущей цифрой
            auto data = neon_data_t(light_allowed ? 
                HMI_SAT_MAX : 
                HMI_SAT_MIN);
            
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                set(i, data);
        }
    public:
        // Конструктор по умолчанию
        neon_source_t(void)
        { }
        
        // Разрешение вывода неонок
        void allow(void)
        {
            update(true);
        }
    } neon_source;
    
    // Источник данных для светодиодов
    class led_source_t : public led_model_t::source_t
    {
        // Текущая стадия
        uint8_t stage : 7;
        // Нужно ли обновлять данные
        bool update_needed : 1;
        
        // Обнволение с указанием текущей стадии
        void update(uint8_t _stage)
        {
            stage = _stage;
            update_needed = true;
        }
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_t::attached();
            // Сброс состояний
            update(0);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            source_t::refresh();
            
            // Нужно ли обновлять данные
            if (!update_needed)
                return;
            update_needed = false;
            
            // Относительно стадии указываем цвет
            switch (stage)
            {
                // Всё обнулить
                case 0:
                case 8:
                    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                        set(i, HMI_COLOR_RGB_BLACK);
                    break;
                // Появление красного в центре
                case 1:
                    set(0, HMI_COLOR_RGB_GREEN);
                    set(5, HMI_COLOR_RGB_GREEN);
                    set(1, HMI_COLOR_RGB_BLUE);
                    set(4, HMI_COLOR_RGB_BLUE);
                    set(2, HMI_COLOR_RGB_RED);
                    set(3, HMI_COLOR_RGB_RED);
                    break;
                // Первый сдвиг
                case 2:
                    set(0, HMI_COLOR_RGB_BLUE);
                    set(5, HMI_COLOR_RGB_BLUE);
                    set(1, HMI_COLOR_RGB_RED);
                    set(4, HMI_COLOR_RGB_RED);
                    set(2, HMI_COLOR_RGB_GREEN);
                    set(3, HMI_COLOR_RGB_GREEN);
                    break;
                case 3:
                    set(0, HMI_COLOR_RGB_RED);
                    set(5, HMI_COLOR_RGB_RED);
                    set(1, HMI_COLOR_RGB_GREEN);
                    set(4, HMI_COLOR_RGB_GREEN);
                    set(2, HMI_COLOR_RGB_BLUE);
                    set(3, HMI_COLOR_RGB_BLUE);
                    break;
                case 4:
                    set(0, HMI_COLOR_RGB_RED);
                    set(1, HMI_COLOR_RGB_RED);
                    set(2, HMI_COLOR_RGB_GREEN);
                    set(3, HMI_COLOR_RGB_GREEN);
                    set(4, HMI_COLOR_RGB_BLUE);
                    set(5, HMI_COLOR_RGB_BLUE);
                    break;
                case 5:
                    set(0, HMI_COLOR_RGB_GREEN);
                    set(1, HMI_COLOR_RGB_GREEN);
                    set(2, HMI_COLOR_RGB_BLUE);
                    set(3, HMI_COLOR_RGB_BLUE);
                    set(4, HMI_COLOR_RGB_RED);
                    set(5, HMI_COLOR_RGB_RED);
                    break;
                case 6:
                    set(0, HMI_COLOR_RGB_BLUE);
                    set(1, HMI_COLOR_RGB_BLUE);
                    set(2, HMI_COLOR_RGB_RED);
                    set(3, HMI_COLOR_RGB_RED);
                    set(4, HMI_COLOR_RGB_GREEN);
                    set(5, HMI_COLOR_RGB_GREEN);
                    break;
                case 7:
                    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                        set(i, HMI_COLOR_RGB_WHITE);
                    break;
                default:
                    assert(false);
                    break;
            }
        }
    public:
        // Конструктор по умолчанию
        led_source_t(void)
        { }
        
        // К следующему состоянию
        bool next(void)
        {
            if (stage >= 8)
                return false;
            
            update(stage + 1);
            return true;
        }
    } led_source;
    
    // Указывает, был ли показан тест
    bool shown = false;
    // Контроллер фреймов
    hmi_frame_controller_t frame;
    // Фильтр смены цвета светодиодов
    display_led_smooth_filter_t led_smooth;
    // Фильтр смены состояния неонок
    display_neon_smooth_filter_t neon_smooth;
    // Фильтр смены цифр в самом начале
    display_nixie_smooth_val_filter_t nixie_start_smooth_val;
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        return !shown;
    }
    
    // Событие установки сцены на дисплей
    virtual void activated(void) override final
    {
        // Базовый метод
        screen_scene_t::activated();
        
        // Сброс номера фрймов
        frame.reset();
        // Выставление фильтров
        nixie.attach(nixie_start_smooth_val);
        // Указываем, что тест уже выводили
        shown = true;
    }
    
    // Обновление сцены
    virtual void refresh(void) override final
    {
        // Базовый метод
        screen_scene_t::refresh();
        
        // Обработка фрейма
        frame++;
        // Если наступило пол секунды
        if (frame.half_seconds_period_get() != 0)
            return;
        // Номер секунды
        auto half_second = frame.half_seconds_get();
        
        // Обработка первой секунды
        if (half_second < 1)
            return;
        
        if (half_second == 1)
        {
            // Убираем эффект появления
            nixie.detach(nixie_start_smooth_val);
            // Разрешаем вывод неонок
            neon_source.allow();
            return;
        }
        
        // Переход к следующей цифре
        nixie_source.next();
        // Начинаем менять подсветку со второй секунды
        if (half_second == 2)
            return;
        
        // Переход к следующей стадии подсветки
        if (led_source.next())
            return;
        
        // Конец сцены
        display_scene_set_default();
    }
public:
    // Конструктор по умолчанию
    display_scene_test_t(void)
    {
        // Лампы
        nixie.attach(nixie_source);
        // Неонки
        neon.attach(neon_source);
        neon.attach(neon_smooth);
        // Светодиоды
        led.attach(led_source);
        led.attach(led_smooth);
        
        led_smooth.frame_count = HMI_FRAME_RATE / 4;
    }
} display_scene_test;

// Сцена вывода часов
static class display_scene_clock_t : public display_scene_t
{
public:
    // Управление лампами
    class nixie_source_t : public nixie_model_t::source_t
    {
        // Хранит, необходмио ли обноить данные
        bool update_needed;
        
        // Смена части (час, минута или секунда)
        void out(hmi_rank_t index, uint8_t value)
        {
            set(index + 0, nixie_data_t(HMI_SAT_MAX, value / 10, true));
            set(index + 1, nixie_data_t(HMI_SAT_MAX, value % 10, true));
        }
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        { 
            // Базовый метод
            source_t::attached();
            
            // Обновление состояния
            second();
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            source_t::refresh();
            
            // Нужно ли обновлять данные
            if (!update_needed)
                return;
            update_needed = false;
            
            // Получаем текущее время
            datetime_t time;
            rtc_datetime_get(time);
            // Вывод относительно типа
            switch (kind)
            {
                case KIND_TIME:
                    // Часы
                    out(0, time.hour);
                    // Минуты
                    out(2, time.minute);
                    // Секунды
                    out(4, time.second);
                    break;
                case KIND_DATE:
                    // День
                    out(0, time.day);
                    // Месяц
                    out(2, time.month);
                    // Год
                    out(4, time.year);
                    break;
                default:
                    assert(false);
                    break;
            }
        }
    public:
        // Вид отображаемых данных
        enum kind_t
        {
            // Время
            KIND_TIME = 0,
            // Дата
            KIND_DATE,
        } kind = KIND_TIME;

        // Конструктор по умолчанию
        nixie_source_t(void)
        { }

        // Секундное событие
        void second(void)
        {
            update_needed = true;
        }
    } nixie_source;    

    // Управление неонками
    class neon_source_t : public neon_model_t::source_t
    {
        // Контроллер фреймов
        hmi_frame_controller_t frame;
        // Текущее состояние
        bool current_state : 1;
        // Указывает, необходмио ли обноить данные неонок
        bool update_needed : 1;
        // Указывает, необходмио ли переключить состояния неонок
        bool toggle_needed : 1;

        // Обновление состояния
        void update(bool toggle)
        {
            toggle_needed = toggle;
            update_needed = true;
        }
        
        // Задает текущее состояния
        void state_set(hmi_rank_t index, bool state)
        {
            set(index, neon_data_t(state ? HMI_SAT_MAX : HMI_SAT_MIN));
        }
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_t::attached();
            
            // Обновление состояния, переключать неонки не нужно
            update(false);
            // Сброс фрейма
            frame.reset();
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            source_t::refresh();
            
            // Обработка номера фрема для полусекундного переключения
            if (++frame == HMI_FRAME_RATE_HALF)
            {
                if (toggle_freq > TOGGLE_FREQ_1HZ) 
                    update(true);
                else
                    frame.reset();
            }
            
            // Нужно ли обновлять данные
            if (!update_needed)
                return;
            update_needed = false;
            
            // Если нужно переключить
            if (toggle_needed)
            {
                // Инверсия
                current_state = !current_state;
                // Задаем новое состояние
                switch (toggle_kind)
                {
                    case TOGGLE_KIND_NONE:
                        for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                            state_set(i, false);
                        break;
                        
                    case TOGGLE_KIND_ALL:
                        for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                            state_set(i, true);
                        break;
                        
                    case TOGGLE_KIND_FULL:
                        for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                            state_set(i, current_state);
                        break;
                        
                    case TOGGLE_KIND_DIAG:
                        state_set(0, current_state);
                        state_set(1, !current_state);
                        state_set(2, !current_state);
                        state_set(3, current_state);
                        break;
                        
                    case TOGGLE_KIND_HORZ:
                        state_set(0, current_state);
                        state_set(1, current_state);
                        state_set(2, !current_state);
                        state_set(3, !current_state);
                        break;
                        
                    case TOGGLE_KIND_VERT:
                        state_set(0, current_state);
                        state_set(1, !current_state);
                        state_set(2, current_state);
                        state_set(3, !current_state);
                        break;
                        
                    default:
                        assert(false);
                        break;
                }
                return;
            }
            // Если нужно всё сбросить
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                state_set(i, false);
        }
    public:
        // Частота смены состояния
        enum toggle_freq_t
        {
            // 1 Гц
            TOGGLE_FREQ_1HZ,
            // 2 Гц
            TOGGLE_FREQ_2HZ,
        } toggle_freq = TOGGLE_FREQ_1HZ;
    
        // Вид переключения
        enum toggle_kind_t
        {
            // Отключить всё
            TOGGLE_KIND_NONE,
            // Включить всё
            TOGGLE_KIND_ALL,
            // Все сразу (как обычно)
            TOGGLE_KIND_FULL,
            // По даигонали
            TOGGLE_KIND_DIAG,
            // По горизонтали
            TOGGLE_KIND_HORZ,
            // По вертикали
            TOGGLE_KIND_VERT,
        } toggle_kind = TOGGLE_KIND_FULL;
        
        // Конструктор по умолчанию
        neon_source_t(void)
        { }

        // Секундное событие
        void second(void)
        {
            // Перключение
            update(true);
            // Сброс фреймов
            frame.reset();
        }
    } neon_source;    
    
    // Фильтр смены цифр ламп
    display_nixie_smooth_val_filter_t nixie_smooth_val;
    // Фильтр смены состояния неонок
    display_neon_smooth_filter_t neon_smooth;
    
    // Управление светодиодами
    display_led_rainbow_source_t led_source;
    // Фильтр смены свечения светодиодов
    display_led_smooth_filter_t led_smooth;

    // Конструктор по умолчанию
    display_scene_clock_t(void)
    {
        // Лампы
        nixie.attach(nixie_source);
        nixie.attach(nixie_smooth_val);
        // Неонки
        neon.attach(neon_source);
        neon.attach(neon_smooth);
        // Светодиоды
        led.attach(led_source);
        led.attach(led_smooth);
    }
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        // Всегда нужно показывать
        return true;
    }
    
    // Секундное событие
    virtual void second(void) override final
    {
        // Базовый метод
        screen_scene_t::second();
        // Оповещение источников
        neon_source.second();
        nixie_source.second();
        // Пробуем установить другую сцену
        display_scene_set_default();
    }
} display_scene_clock;

/*

#include "light.h"

// Сцена вывода освещенности
static class display_scene_light_t : public display_scene_t
{
    static hmi_sat_t light_sat_get(void)
    {
        static const xmath_point2d_t<float32_t, hmi_sat_t> POINTS[] =
        {
            { 10.0f,  40 },
            { 20.0f,  75 },
            { 30.0f,  110 },
            { 40.0f,  145 },
            { 50.0f,  180 },
            { 60.0f,  215 },
            { 70.0f,  255 },
        };
        
        const auto value = light_current_get();
        
        return value != SENSOR_VALUE_EMPTY ?
            xmath_linear_interpolation(value, POINTS, ARRAY_SIZE(POINTS)) :
            HMI_SAT_MAX;
    }

    class nixie_light_t : public nixie_filter_t
    {
    public:
        // Конструктор по умолчанию
        nixie_light_t(void) : nixie_filter_t(HMI_FILTER_PURPOSE_INSTANT_SAT)
        { }
        
        hmi_sat_t last_sat = HMI_SAT_MAX;
        
    protected:
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            nixie_filter_t::refresh();
            
            // Нужно ли обновлять данные
            auto cur_sat = light_sat_get();
            if (last_sat == cur_sat)
                return;
            last_sat = cur_sat;
            // Передача дальше
            reprocess();
        }

        // Событие обработки новых данных
        virtual void process(hmi_rank_t index, nixie_data_t &data) override final
        {
            data.sat = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.sat, last_sat, HMI_SAT_MAX);
            // Базовый метод
            nixie_filter_t::process(index, data);
        }
        
    } nixie_light;
    
    // Фильтр смены цифр ламп
    display_nixie_change_filter_t nixie_change;
public:
    // Управление лампами
    class nixie_source_t : public nixie_filter_t
    {
        // Текущее покзаание
        sensor_value_t last_value;
        
        // Обновление покзаания на экране
        void update(sensor_value_t value)
        {
            last_value = value;
            if (value == SENSOR_VALUE_EMPTY)
            {
                // Нет данных
                for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
                    change_full(i, NIXIE_DIGIT_SPACE);
                return;
            }
            
            auto round = (uint32_t)(value * 10);
            uint32_t rank = 1;
            for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++, rank *= 10)
                change_full(NIXIE_COUNT - i - 1, (uint8_t)((round / rank) % 10));
        }
    protected:
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            nixie_filter_t::refresh();
            
            // Текущее покзаание
            const auto current_value = light_current_get();
            if (current_value != last_value)
                update(current_value);
        }
                
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            nixie_filter_t::attached();
            // Обновление
            update(light_current_get());
        }
    public:
        // Конструктор по умолчанию
        nixie_source_t(void) : nixie_filter_t(HMI_FILTER_PURPOSE_SOURCE)
        { }
    } nixie_source;    

    // Управление неонками
    class neon_source_t : public neon_filter_t
    {
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            neon_filter_t::attached();
            
            // Всё включаем
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                state_set(i, false);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            neon_filter_t::refresh();
        }
    public:
        // Конструктор по умолчанию
        neon_source_t(void) : neon_filter_t(HMI_FILTER_PURPOSE_SOURCE)
        { }
    } neon_source;    
    
    // Конструктор по умолчанию
    display_scene_light_t(void)
    {
        nixie_change.kind = display_nixie_change_filter_t::KIND_SWITCH_DEPTH_OUT;
        // Лампы
        nixie.attach(nixie_source);
        nixie.attach(nixie_change);
        nixie.attach(nixie_light);
        // Неонки
        neon.attach(neon_source);
    }

protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        // Всегда нужно показывать
        return true;
    }
} display_scene_light;

// Тествая сцена
static class display_scene_testing_t : public display_scene_t
{
    class led_source_t : public led_filter_t
    {
    public:
        // Конструктор по умолчанию
        led_source_t(void) : led_filter_t(HMI_FILTER_PURPOSE_SOURCE)
        { }
        
        uint32_t prescaler = 0;
        hmi_sat_t value = 0;
        
    protected:
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            led_filter_t::refresh();
            
            if (++prescaler < 2)
                return;
            prescaler = 0;
            value++;
            
            reprocess();
        }

        // Событие обработки новых данных
        virtual void process(hmi_rank_t index, hmi_rgb_t &data) override final
        {
            data = hmi_hsv_t(0, 255, value).to_rgb();
            
            // Базовый метод
            led_filter_t::process(index, data);
        }
        
    } led_source;
public:
    // Конструктор по умолчанию
    display_scene_testing_t(void)
    {
        led.attach(led_source);
    }

protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        // Всегда нужно показывать
        return true;
    }
} display_scene_testing;

// Тестовый вывод на дисплей
static class display_nixie_source_test_t : public nixie_filter_t
{
    uint8_t frame = 0;
protected:
    // Событие присоединения к цепочке
    virtual void attached(void) override final
    {
        frame = 0;
    }
    
    // Обнвление данных
    virtual void refresh() override final
    {
        if (frame > 0)
        {
            frame--;
            return;
        }
        frame = HMI_FRAME_RATE;
        for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
            change_full(i, rand() % 10);
    }
public:
    // Конструктор по умолчанию
    display_nixie_source_test_t(void) : nixie_filter_t(HMI_FILTER_PURPOSE_SOURCE)
    { }
} display_nixie_source_test;

*/

#include "light.h"

// Сервис управления освещенностью
class display_ambient_service_t
{
    // Текущее значение яркости
    static light_level_t light_level;
    
    // Класс управления лампами
    static class nixie_control_t : public nixie_filter_t
    {
    public:
        // Конструктор по умолчанию
        nixie_control_t(void) : nixie_filter_t(PRIORITY_AMBIENT)
        { }
        
    protected:
        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const override final
        {
            // Фильтр перемещать нельзя
            return false;
        }

        // Трансформация данных
        virtual void transform(hmi_rank_t index, nixie_data_t &data) const override final
        {
            // Базовый метод
            nixie_filter_t::transform(index, data);
            
            // Преобразование
            constexpr const light_level_t DX = 19;
            data.sat = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.sat, light_level + DX, LIGHT_LEVEL_MAX + DX);
        }
    } nixie_control;

    // Класс управления неонками
    static class neon_control_t : public neon_filter_t
    {
    public:
        // Конструктор по умолчанию
        neon_control_t(void) : neon_filter_t(PRIORITY_AMBIENT)
        { }
        
    protected:
        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const override final
        {
            // Фильтр перемещать нельзя
            return false;
        }

        // Трансформация данных
        virtual void transform(hmi_rank_t index, neon_data_t &data) const override final
        {
            // Базовый метод
            neon_filter_t::transform(index, data);
            
            // Преобразование
            constexpr const light_level_t DX = 17;
            data.sat = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.sat, light_level + DX, LIGHT_LEVEL_MAX + DX);
        }
    } neon_control;
    
    // Класс управления светодиодами
    static class led_control_t : public led_filter_t
    {
    public:
        // Конструктор по умолчанию
        led_control_t(void) : led_filter_t(PRIORITY_AMBIENT)
        { }
        
    protected:
        // Обнвление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            led_filter_t::refresh();
            
            // Обновление освещенности
            light_level_refresh();
        }
    
        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const override final
        {
            // Фильтр перемещать нельзя
            return false;
        }

        // Трансформация данных
        virtual void transform(hmi_rank_t index, hmi_rgb_t &data) const override final
        {
            // Базовый метод
            led_filter_t::transform(index, data);
            
            // Преобразование
            constexpr const light_level_t DX = 10;
            data.r = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.r, light_level + DX, LIGHT_LEVEL_MAX + DX);
            data.g = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.g, light_level + DX, LIGHT_LEVEL_MAX + DX);
            data.b = xmath_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.b, light_level + DX, LIGHT_LEVEL_MAX + DX);
        }
    } led_control;
    
    // Обновление данных освещенности
    static void light_level_refresh(void)
    {
        // Текущее значение уровня освещенности
        const auto level = light_level_get();
        if (light_level == level)
            return;
        
        // Применение нового значения
        light_level = level;
        
        // Оповещение фильтров
        led_control.reoutput();
        neon_control.reoutput();
        nixie_control.reoutput();
    }
    
    // Класс статический
    display_ambient_service_t(void)
    { }
    
public:
    // Установка фильтров
    static void setup()
    {
        screen.led.attach(led_control);
        screen.neon.attach(neon_control);
        screen.nixie.attach(nixie_control);
    }
};

// Инициализация статики
light_level_t display_ambient_service_t::light_level;
display_ambient_service_t::led_control_t display_ambient_service_t::led_control;
display_ambient_service_t::neon_control_t display_ambient_service_t::neon_control;
display_ambient_service_t::nixie_control_t display_ambient_service_t::nixie_control;

// Установка сцены по умолчанию
static void display_scene_set_default(void)
{
    // Список известных сцен
    static display_scene_t * const SCENES[] =
    {
        &display_scene_test,
        //&display_scene_light,
        //&display_scene_testing,
        &display_scene_clock
    };
    // Обход известных сцен
    for (uint8_t i = 0; i < ARRAY_SIZE(SCENES); i++)
        if (SCENES[i]->show_required())
        {
            screen.scene_set(SCENES[i]);
            return;
        }
    // Такое недопустимо
    assert(false);
}

void display_init(void)
{
    display_ambient_service_t::setup();
    
    display_scene_set_default();
}
