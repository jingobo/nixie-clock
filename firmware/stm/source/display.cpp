#include "rtc.h"
#include "timer.h"
#include "screen.h"
#include "display.h"

// Количество фреймов смены состояния
#define DISPLAY_SMOOTH_FRAME_COUNT          26
// Количество фреймов смены состояния (пополам)
#define DISPLAY_SMOOTH_FRAME_COUNT_HALF     (DISPLAY_SMOOTH_FRAME_COUNT / 2)

// Класс фильтра смены цифр на лампах
class display_nixie_change_filter_t : public nixie_filter_t
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
            // Фреймы исчезновения старой цифры
            if (e.frame < DISPLAY_SMOOTH_FRAME_COUNT_HALF)
                change_sat(i, e.digit_from, hmi_drift(HMI_SAT_MAX, HMI_SAT_MIN, e.frame, DISPLAY_SMOOTH_FRAME_COUNT_HALF));
            // Фреймы появления новой цифры
            else if (e.frame < DISPLAY_SMOOTH_FRAME_COUNT)
                change_sat(i, e.digit_to, hmi_drift(HMI_SAT_MIN, HMI_SAT_MAX, e.frame - DISPLAY_SMOOTH_FRAME_COUNT_HALF, DISPLAY_SMOOTH_FRAME_COUNT_HALF));
            else
            {
                // Завершение эффекта
                change_sat(i, e.digit_to, HMI_SAT_MAX);
                e.stop();
            }
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
            // Фреймы смены цифры
            if (e.frame < DISPLAY_SMOOTH_FRAME_COUNT)
            {
                auto sat = hmi_drift(HMI_SAT_MIN, HMI_SAT_MAX, e.frame, DISPLAY_SMOOTH_FRAME_COUNT);
                if (e.frame & 1)
                    change_sat(i, e.digit_to, sat);
                else
                    change_sat(i, e.digit_from, HMI_SAT_MAX - sat);
            }
            // Фреймы довода насыщенности
            else if (e.frame < DISPLAY_SMOOTH_FRAME_COUNT + 5)
                change_sat(i, e.digit_to, hmi_drift(180, HMI_SAT_MAX, e.frame - DISPLAY_SMOOTH_FRAME_COUNT, 5));
            else
            {
                // Завершение эффекта
                change_sat(i, e.digit_to, HMI_SAT_MAX);
                e.stop();
            }
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
            change_sat(i, e.digit_from, HMI_SAT_MAX);
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
    virtual void process(hmi_rank_t index, nixie_data_t &data) override final
    {
        auto &e = effect[index];
        // Проверка, изменилась ли цифра
        auto last = data_get(index);
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
        nixie_filter_t::process(index, data);
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
    display_nixie_change_filter_t(void) : nixie_filter_t(HMI_FILTER_PURPOSE_SMOOTH_VAL)
    { }
};

// Класс фильтра плавной смены состояния неонок
class display_neon_change_filter_t : public neon_filter_t
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
        neon_data_t data;
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
            change(i, hmi_drift(e.sat_from, e.sat_to, e.frame, DISPLAY_SMOOTH_FRAME_COUNT));
            // Инкремент фрейма, и проверка на завершение эффекта
            if (++e.frame > DISPLAY_SMOOTH_FRAME_COUNT)
                e.stop();
        }
    }
    
    // Событие обработки новых данных
    virtual void process(hmi_rank_t index, neon_data_t &data) override final
    {
        auto &e = effect[index];
        // Проверка, изменилась ли насыщенность
        auto last = data_get(index);
        if (data != last)
        {
            // Старт эффекта
            e.start(last.sat, data.sat);
            // Применяем старые данные
            data = last;
        }
        // Базовый метод
        neon_filter_t::process(index, data);
    }
public:
    // Конструктор по умолчанию
    display_neon_change_filter_t(void) : neon_filter_t(HMI_FILTER_PURPOSE_SMOOTH_VAL)
    { }
};

// Класс фильтра плавной смены цвета светододов
class display_led_change_filter_t : public led_filter_t
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
        led_filter_t::refresh();
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
            hmi_rgb_t tmp(hmi_drift(e.color_from.r, e.color_to.r, e.frame, frame_count),
                          hmi_drift(e.color_from.g, e.color_to.g, e.frame, frame_count),
                          hmi_drift(e.color_from.b, e.color_to.b, e.frame, frame_count));
            data_change(i, tmp);
            // Инкремент фрейма и проверка на завершение эффекта
            if (++e.frame > frame_count)
                e.stop();
        }
    }
    
    // Событие обработки новых данных
    virtual void process(hmi_rank_t index, hmi_rgb_t &data) override final
    {
        auto &e = effect[index];
        // Проверка, изменился ли цвет
        auto last = data_get(index);
        if (data != last)
        {
            // Старт эффекта
            e.start(last, data);
            // Применяем старый цвет
            data = last;
        }
        // Базовый метод
        led_filter_t::process(index, data);
    }
public:
    // Количество фреймов при изменении цвета
    uint32_t frame_count = DISPLAY_SMOOTH_FRAME_COUNT;
    // Конструктор по умолчанию
    display_led_change_filter_t(void) : led_filter_t(HMI_FILTER_PURPOSE_SMOOTH_VAL)
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
    class nixie_source_t : public nixie_filter_t
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
            nixie_filter_t::attached();
            // Начинаем с нулевой цифры
            digit_set(0);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            nixie_filter_t::refresh();
            // Нужно ли обновлять данные
            if (!update_needed)
                return;
            update_needed = false;
            // Установка всех разрядов текущей цифрой
            for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
                change_full(i, digit, HMI_SAT_MAX, true);
        }
    public:
        // Конструктор по умолчанию
        nixie_source_t(void) : nixie_filter_t(HMI_FILTER_PURPOSE_SOURCE)
        { }
        
        // Переход к следующей цифре
        void next(void)
        {
            if (digit < 9)
                digit_set(digit + 1);
        }
    } nixie_source;
    
    // Источник данных для неонок
    class neon_source_t : public neon_filter_t
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
            neon_filter_t::attached();
            // Сброс состояний
            update(false);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            neon_filter_t::refresh();
            // Нужно ли обновлять данные
            if (!update_needed)
                return;
            update_needed = false;
            // Установка всех разрядов текущей цифрой
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                state_set(i, light_allowed);
        }
    public:
        // Конструктор по умолчанию
        neon_source_t(void) : neon_filter_t(HMI_FILTER_PURPOSE_SOURCE)
        { }
        
        // Разрешение вывода неонок
        void allow(void)
        {
            update(true);
        }
    } neon_source;
    
    // Источник данных для светодиодов
    class led_source_t : public led_filter_t
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
            led_filter_t::attached();
            // Сброс состояний
            update(0);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            led_filter_t::refresh();
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
                        data_change(i, HMI_COLOR_RGB_BLACK);
                    break;
                // Появление красного в центре
                case 1:
                    data_change(0, HMI_COLOR_RGB_GREEN);
                    data_change(5, HMI_COLOR_RGB_GREEN);
                    data_change(1, HMI_COLOR_RGB_BLUE);
                    data_change(4, HMI_COLOR_RGB_BLUE);
                    data_change(2, HMI_COLOR_RGB_RED);
                    data_change(3, HMI_COLOR_RGB_RED);
                    break;
                // Первый сдвиг
                case 2:
                    data_change(0, HMI_COLOR_RGB_BLUE);
                    data_change(5, HMI_COLOR_RGB_BLUE);
                    data_change(1, HMI_COLOR_RGB_RED);
                    data_change(4, HMI_COLOR_RGB_RED);
                    data_change(2, HMI_COLOR_RGB_GREEN);
                    data_change(3, HMI_COLOR_RGB_GREEN);
                    break;
                case 3:
                    data_change(0, HMI_COLOR_RGB_RED);
                    data_change(5, HMI_COLOR_RGB_RED);
                    data_change(1, HMI_COLOR_RGB_GREEN);
                    data_change(4, HMI_COLOR_RGB_GREEN);
                    data_change(2, HMI_COLOR_RGB_BLUE);
                    data_change(3, HMI_COLOR_RGB_BLUE);
                    break;
                case 4:
                    data_change(0, HMI_COLOR_RGB_RED);
                    data_change(1, HMI_COLOR_RGB_RED);
                    data_change(2, HMI_COLOR_RGB_GREEN);
                    data_change(3, HMI_COLOR_RGB_GREEN);
                    data_change(4, HMI_COLOR_RGB_BLUE);
                    data_change(5, HMI_COLOR_RGB_BLUE);
                    break;
                case 5:
                    data_change(0, HMI_COLOR_RGB_GREEN);
                    data_change(1, HMI_COLOR_RGB_GREEN);
                    data_change(2, HMI_COLOR_RGB_BLUE);
                    data_change(3, HMI_COLOR_RGB_BLUE);
                    data_change(4, HMI_COLOR_RGB_RED);
                    data_change(5, HMI_COLOR_RGB_RED);
                    break;
                case 6:
                    data_change(0, HMI_COLOR_RGB_BLUE);
                    data_change(1, HMI_COLOR_RGB_BLUE);
                    data_change(2, HMI_COLOR_RGB_RED);
                    data_change(3, HMI_COLOR_RGB_RED);
                    data_change(4, HMI_COLOR_RGB_GREEN);
                    data_change(5, HMI_COLOR_RGB_GREEN);
                    break;
                case 7:
                    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                        data_change(i, HMI_COLOR_RGB_WHITE);
                    break;
                    break;
                default:
                    assert(false);
                    break;
            }
        }
    public:
        // Конструктор по умолчанию
        led_source_t(void) : led_filter_t(HMI_FILTER_PURPOSE_SOURCE)
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
    display_led_change_filter_t led_change;
    // Фильтр смены состояний неонок
    display_neon_change_filter_t neon_change;
    // Фильтр смены цифр в самом начале
    display_nixie_change_filter_t nixie_start_change;
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
        nixie.attach(nixie_start_change);
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
            nixie.detach(nixie_start_change);
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
        neon.attach(neon_change);
        // Светодиоды
        led.attach(led_source);
        led.attach(led_change);
        led_change.frame_count = HMI_FRAME_RATE / 4;
    }
} display_scene_test;

// Сцена вывода часов
static class display_scene_clock_t : public display_scene_t
{
public:
    // Управление лампами
    class nixie_source_t : public nixie_filter_t
    {
        // Хранит, необходмио ли обноить данные
        bool update_needed;
        
        // Смена части (час, минута или секунда)
        void out(hmi_rank_t index, uint8_t value)
        {
            change_full(index + 0, value / 10);
            change_full(index + 1, value % 10);
        }
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        { 
            // Базовый метод
            nixie_filter_t::attached();
            // Обновление состояния
            second();
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            nixie_filter_t::refresh();
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
        nixie_source_t(void) : nixie_filter_t(HMI_FILTER_PURPOSE_SOURCE)
        { }

        // Секундное событие
        void second(void)
        {
            update_needed = true;
        }
    } nixie_source;    

    // Управление неонками
    class neon_source_t : public neon_filter_t
    {
        // Контроллер фреймов
        hmi_frame_controller_t frame;
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
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            neon_filter_t::attached();
            // Обновление состояния, переключать неонки не нужно
            update(false);
            // Сброс фрейма
            frame.reset();
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            neon_filter_t::refresh();
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
                // Получаем текущее состояние
                auto cs = state_get(0);
                // Инверсия
                cs = !cs;
                // Задаем новое состояние
                switch (toggle_kind)
                {
                    case TOGGLE_KIND_NONE:
                        // Ничего
                        break;
                    case TOGGLE_KIND_FULL:
                        for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                            state_set(i, cs);
                        break;
                    case TOGGLE_KIND_DIAG:
                        state_set(0, cs);
                        state_set(1, !cs);
                        state_set(2, !cs);
                        state_set(3, cs);
                        break;
                    case TOGGLE_KIND_HORZ:
                        state_set(0, cs);
                        state_set(1, cs);
                        state_set(2, !cs);
                        state_set(3, !cs);
                        break;
                    case TOGGLE_KIND_VERT:
                        state_set(0, cs);
                        state_set(1, !cs);
                        state_set(2, cs);
                        state_set(3, !cs);
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
            // Не перключать
            TOGGLE_KIND_NONE,
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
        neon_source_t(void) : neon_filter_t(HMI_FILTER_PURPOSE_SOURCE)
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
    display_nixie_change_filter_t nixie_change;
    // Фильтр смены насыщенности неонок
    display_neon_change_filter_t neon_change;

    // Конструктор по умолчанию
    display_scene_clock_t(void)
    {
        // Лампы
        nixie.attach(nixie_source);
        nixie.attach(nixie_change);
        // Неонки
        neon.attach(neon_source);
        neon.attach(neon_change);
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

/*// Тестовый источник для LED эффекта радуги
TODO:
static class display_led_rainbow_source_t : public led_model_t::filter_t
{
    uint8_t time_prescaller, hue;
protected:
    // Обнвление данных
    virtual void refresh(void) override final
    { 
        if (++time_prescaller < HMI_FRAME_RATE / 20)
            return;
        time_prescaller = 0;
        
        hmi_hsv_t hsv(0, HMI_SAT_MAX, HMI_SAT_MAX / 3);
        
        for (hmi_rank_t i = 0; i < LED_COUNT; i++)
        {
            hsv.h = ((hue + i) % 32) * 8;
            data_change(i, hsv.to_rgb());
        }
        hue++;
    }
public:
    // Конструктор по умолчанию
    display_led_rainbow_source_t(void) : filter_t(HMI_FILTER_PURPOSE_SOURCE)
    {
        time_prescaller = HMI_FRAME_RATE;
        hue = 0;
    }
} display_led_rainbow_source;

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

static display_nixie_change_filter_t nixie_switch_test; */

// Установка сцены по умолчанию
static void display_scene_set_default(void)
{
    // Список известных сцен
    static display_scene_t * const SCENES[] =
    {
        &display_scene_test,
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
    display_scene_set_default();
}
