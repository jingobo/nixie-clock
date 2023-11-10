﻿#include "esp.h"
#include "rtc.h"
#include "light.h"
#include "timer.h"
#include "xmath.h"
#include "random.h"
#include "screen.h"
#include "storage.h"
#include "display.h"
#include "proto/display.inc.h"

// Установка сцены по умолчанию (предварительное объявление)
static void display_scene_set_default(void);

// Базовый класс сцены
class display_scene_t : public screen_scene_t
{
    friend void display_scene_set_default(void);
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) = 0;
};

// Базовый класс настраиваемой сцены
template <typename SETTINGS, typename COMMAND_GET, typename COMMAND_SET>
class display_scene_ipc_t : public display_scene_t
{
    // Обработчик команды чтения
    class getter_t : public ipc_responder_t
    {
        // Комадна
        COMMAND_GET command;
        // Сцена
        display_scene_ipc_t &scene;
    protected:
        // Получает ссылку на команду
        virtual ipc_command_t &command_get(void) override final
        {
            return command;
        }
        
        // Событие обработки данных
        virtual void work(bool idle) override final
        {
            if (idle)
                return;
            
            // Заполняем ответ
            command.response = scene.settings;
            // Передача ответа
            transmit();
        }
        
    public:
        // Конструктор по умолчанию
        getter_t(display_scene_ipc_t &_scene) : scene(_scene)
        { }
    } getter;
    
    // Обработчик команды записи
    class setter_t : public ipc_responder_t
    {
        // Комадна
        COMMAND_SET command;
        // Сцена
        display_scene_ipc_t &scene;
    protected:
        // Получает ссылку на команду
        virtual ipc_command_t &command_get(void) override final
        {
            return command;
        }
        
        // Событие обработки данных
        virtual void work(bool idle) override final
        {
            if (idle)
                return;
            
            // Применение настроек
            scene.settings_old = scene.settings;
            scene.settings = command.request;
            scene.settings_changed();
            storage_modified();
            
            // Передача ответа
            transmit();
        }
        
    public:
        // Конструктор по умолчанию
        setter_t(display_scene_ipc_t &_scene) : scene(_scene)
        { }
    } setter;
    
    // Источник светодиодов
    led_source_t led_source;
    // Источник неонок
    neon_source_t neon_source;
    // Фильтр плавной смены цифр на лампах
    nixie_switcher_t nixie_switcher;
    // Таймаут предварительного просмотра в секундах
    uint8_t show_preview_timeout = 0;

    // Получает ссылку на типизированные настройки
    static display_settings_t& settings_get(SETTINGS &settings)
    {
        return (display_settings_t&)settings;
    }
    
protected:
    // Ссылка на текущие настройки
    SETTINGS &settings;
    // Ссылка на предыдущие настройки
    SETTINGS settings_old;

    // Обработчик примнения настроек
    virtual void settings_apply(bool initial)
    {
        const auto &old = settings_get(settings_old);
        led_source.settings_apply(initial ? NULL : &old.led);
        neon_source.settings_apply(initial ? NULL : &old.neon);
    }
    
    // Обработчик изменения настроек
    virtual void settings_changed(void)
    {
        // Приминение настроек
        settings_apply(false);
        
        // Пробуем показать
        show_preview_timeout = 10;
        display_scene_set_default();
    }
    
    // Секундное событие
    virtual void second(void) override
    {
        // Базовый метод
        screen_scene_t::second();
        
        // Оповещение источников
        led_source.second();
        neon_source.second();
        // Пробуем установить другую сцену
        display_scene_set_default();
        
        // Таймаут предварительного просмотра
        if (show_preview_timeout > 0 && --show_preview_timeout <= 0)
            display_scene_set_default();
    }
    
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override
    {
        // По умолчанию пока идет предпросмотр
        return show_preview_timeout > 0;
    }
    
public:
    // Конструктор по умолчанию
    display_scene_ipc_t(SETTINGS &_settings) : 
        settings(_settings),
        settings_old(_settings),
        setter(*this), getter(*this), 
        led_source(settings_get(_settings).led),
        neon_source(settings_get(_settings).neon), 
        nixie_switcher(settings_get(_settings).nixie)
    {
        led.attach(led_source);
        neon.attach(neon_source);
        nixie.attach(nixie_switcher);
    }
    
    // Установка обработчиков команд
    void setup(void)
    {
        esp_handler_add(getter);
        esp_handler_add(setter);
    }
};

// Базовый класс настраиваемой сцены с опцией показа
template <typename SETTINGS, typename COMMAND_GET, typename COMMAND_SET>
class display_scene_arm_t : public display_scene_ipc_t<SETTINGS, COMMAND_GET, COMMAND_SET>
{
    // Базовый класс
    using base_t = display_scene_ipc_t<SETTINGS, COMMAND_GET, COMMAND_SET>;

    // Получает ссылку на типизированные настройки
    static display_settings_arm_t& settings_get(SETTINGS &settings)
    {
        return (display_settings_arm_t&)settings;
    }
    
protected:
    // Получает признак возможности вывода сцены
    virtual bool show_allowed(void)
    {
        return settings_get(base_t::settings).allow;
    }
    
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        // Если вывод разрешен
        return base_t::show_required() || show_allowed();
    }
    
public:
    // Конструктор по умолчанию
    display_scene_arm_t(SETTINGS &_settings) : base_t(_settings)
    { }
};

// Базовый класс настраиваемой сцены с опцией таймаута
template <typename SETTINGS, typename COMMAND_GET, typename COMMAND_SET>
class display_scene_timeout_t : public display_scene_arm_t<SETTINGS, COMMAND_GET, COMMAND_SET>
{
    // Базовый класс
    using base_t = display_scene_arm_t<SETTINGS, COMMAND_GET, COMMAND_SET>;

    // Получает ссылку на типизированные настройки
    static display_settings_timeout_t& settings_get(SETTINGS &settings)
    {
        return (display_settings_timeout_t&)settings;
    }
    
    // Секундный таймаут запроса на показ
    uint8_t show_request_timeout;
    
    // Получает признак активности таймаута запроса на показ
    bool show_request_timeout_active(void) const
    {
        return show_request_timeout > 0;
    }
    
protected:
    // Обработчик примнения настроек
    virtual void settings_apply(bool initial) override
    {
        // Базовый метод
        base_t::settings_apply(initial);
        
        // Сброс таймаута показа
        show_request_timeout = 0;
    }

    // Секундное событие
    virtual void second(void) override
    {
        // Базовый метод
        base_t::second();
        
        // Таймаут показа
        if (show_request_timeout_active() && --show_request_timeout <= 0)
            display_scene_set_default();
    }
    
    // Получает признак возможности вывода сцены
    virtual bool show_allowed(void) override final
    {
        return base_t::show_allowed() &&
               show_request_timeout_active();
    }
    
    // Запрос на показ
    void show_request(void)
    {
        show_request_timeout = settings_get(base_t::settings).timeout;
        display_scene_set_default();
    }
    
public:
    // Конструктор по умолчанию
    display_scene_timeout_t(SETTINGS &_settings) : base_t(_settings)
    { }
};

// Сцена отображения времени
static class display_scene_time_t : public display_scene_ipc_t<display_settings_t, display_command_time_get_t, display_command_time_set_t>
{
    // Управление лампами
    class nixie_source_t : public nixie_number_source_t
    {
        // Признак вывода данных
        bool updated = false;
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
            if (updated)
                return;
            
            // Обновление
            updated = true;

            // Часы
            out(0, 2, rtc_time.hour);
            // Минуты
            out(2, 2, rtc_time.minute);
            // Секунды
            out(4, 2, rtc_time.second);
        }
    public:
        // Секундное событие
        void second(void)
        {
            updated = false;
        }
    } nixie_source;
    
    // Текущие настройки
    static display_settings_t settings;
    
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        // Базовый метод не вызывается (показывается всегда по умолчанию)
        return true;
    }
    
    // Секундное событие
    virtual void second(void) override final
    {
        // Базовый метод
        display_scene_ipc_t::second();
        // Оповещение источников
        nixie_source.second();
    }
public:
    // Конструктор по умолчанию
    display_scene_time_t(void) : display_scene_ipc_t(settings)
    {
        // Лампы
        nixie.attach(nixie_source);
        
        // Финальное применение настроек
        settings_apply(true);
    }
} display_scene_time;

// Настройки сцены времени
display_settings_t display_scene_time_t::settings @ STORAGE_SECTION =
{
    .led =
    {
        .effect = led_source_t::EFFECT_FLASH,
        .smooth = 20,
        .source = led_source_t::DATA_SOURCE_ANY_RANDOM,
        .rgb =
        {
            hmi_rgb_init(255, 0, 0),
            hmi_rgb_init(255, 255, 0),
            hmi_rgb_init(0, 255, 0),
            hmi_rgb_init(0, 255, 255),
            hmi_rgb_init(0, 0, 255),
            hmi_rgb_init(255, 0, 255),
        },
    },
    
    .neon =
    {
        .mask = neon_source_t::RANK_MASK_ALL,
        .period = 4,
        .smooth = 3,
        .inversion = false,
    },
    
    .nixie =
    {
        .effect = nixie_switcher_t::EFFECT_SWITCH_OUT,
    },
};

// Сцена отображения даты
static class display_scene_date_t : public display_scene_timeout_t<display_settings_timeout_t, display_command_date_get_t, display_command_date_set_t>
{
    // Управление лампами
    class nixie_source_t : public nixie_number_source_t
    {
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        { 
            // Базовый метод
            source_t::attached();

            // День
            out(0, 2, rtc_time.day);
            // Месяц
            out(2, 2, rtc_time.month, true);
            // Год
            out(4, 2, rtc_time.year, true);
        }
    } nixie_source;
    
    // Текущий день недели
    uint8_t week_day;
    
    // Текущие настройки
    static display_settings_timeout_t settings;
    
public:
    // Конструктор по умолчанию
    display_scene_date_t(void) : display_scene_timeout_t(settings)
    {
        // Лампы
        nixie.attach(nixie_source);
        
        // Изначально не показываем
        week_day = rtc_week_day;
        // Финальное применение настроек
        settings_apply(true);
    }
    
    // Обработчик секундного события (вызывается всегда)
    void second_always(void)
    {
        if (week_day == rtc_week_day)
            return;

        // Если наступил новый день
        week_day = rtc_week_day;
        show_request();
    }    
} display_scene_date;

// Настройки сцены даты
display_settings_timeout_t display_scene_date_t::settings @ STORAGE_SECTION =
{
    .base =
    {
        .base =
        {
            .led =
            {
                .effect = led_source_t::EFFECT_FILL,
                .smooth = 4,
                .source = led_source_t::DATA_SOURCE_CUR_RANDOM,
                .rgb =
                {
                    hmi_rgb_init(255, 0, 0),
                    hmi_rgb_init(255, 255, 0),
                    hmi_rgb_init(0, 255, 0),
                    hmi_rgb_init(0, 255, 255),
                    hmi_rgb_init(0, 0, 255),
                    hmi_rgb_init(255, 0, 255),
                },
            },
            
            .neon =
            {
                .mask = neon_source_t::RANK_MASK_NONE,
                .period = 4,
                .smooth = 3,
                .inversion = false,
            },
            
            .nixie =
            {
                .effect = nixie_switcher_t::EFFECT_SMOOTH_SUB,
            },
        },
        .allow = true,
    },
    .timeout = 5,
};

// Сцена прогрева ламп
static class display_scene_heat_t : public display_scene_t
{
    // Общее время проведения прогрева
    static constexpr const uint8_t TOTAL_TIME = 20;
    
    // Структура информации о разряде
    struct rank_t
    {
        // Текущая цифра
        uint8_t digit;
        // Секундный прескалер
        uint8_t prescaler;

        // Конструктор по умолчанию
        rank_t(void)
        { }
        
        // Конструктор для информации о разряде
        constexpr rank_t(uint8_t _digit)
            : digit(_digit), prescaler(TOTAL_TIME / (NIXIE_DIGIT_SPACE - _digit))
        { }
        
        // Сброс в начальное состояние
        void reset(uint8_t digit)
        {
            prescaler = 0;
            this->digit = digit;
        }
    };
    
    // Информация о разрядах
    static const rank_t RANK_INFO[];
    
    // Источник данных для лмап
    class nixie_source_t : public nixie_model_t::source_t
    {
        // Прескалер секунд
        uint32_t frame;
        // Данные по разрядам
        rank_t rank_data[NIXIE_COUNT];
        
        // Вывод данных разрядов
        void flush_ranks(void)
        {
            for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
                out_set(i, nixie_data_t(HMI_SAT_MAX, rank_data[i].digit, true));
        }
        
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_t::attached();
            
            // Сброс прескалера секунд
            frame = 0;
            
            // Сброс данных разрядов в начальное состояние
            for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
                rank_data[i].reset(RANK_INFO[i].digit);
            
            // Вывод данных разрядов
            flush_ranks();
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            source_t::refresh();
            
            // Прескалер секунды
            if (++frame < HMI_FRAME_RATE)
                return;
            frame = 0;
            
            // Количество завершенных разрядов
            auto done_count = 0;
            
            // Секундная обработка разрдов
            for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
            {
                // Данные и информация
                auto &data = rank_data[i];
                auto &info = RANK_INFO[i];
                
                // Прескалер цифры
                if (++data.prescaler < info.prescaler)
                    continue;
                data.prescaler = info.prescaler;

                // Завершение или переход к следующей цифре
                if (data.digit < 9)
                    data.reset(data.digit + 1);
                else
                    done_count++;
            }
            
            // Если не все разряды завершены
            if (done_count < NIXIE_COUNT)
                flush_ranks();
            else
                display_scene_set_default();
        }
    } nixie_source;
    
    // Источник данных для неонок
    class neon_source_t : public neon_model_t::source_t
    {
        // Текущий разряд
        hmi_rank_t rank;
        // Контроллер плавного изменения
        neon_model_t::smoother_t smoother;
        
        // Запуск эффекта разряда
        void rank_start(hmi_rank_t rank)
        {
            this->rank = rank;
            smoother.start(rank, out_get(rank));
        }
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_t::attached();
            
            // Сброс данных разрядов в начальное состояние
            smoother.stop();
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                out_set(i, neon_data_t());
            
            // Запуск пекрвого равзряда
            rank_start(0);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            source_t::refresh();
            
            // Обработка плавности
            auto processing = false;
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                if (smoother.process_needed(i))
                {
                    processing = true;
                    out_set(i, smoother.process(i, neon_data_t(HMI_SAT_MAX)));
                }
            
            // Если разряд обрабатывается
            if (processing)
                return;
            
            // Если есть разряды к обработке
            if (rank < NEON_COUNT - 1)
                rank_start(rank + 1);
        }
    public:
        // Конструктор по умолчанию
        neon_source_t(void)
        {
            // Настройка плавности
            smoother.frame_count_set(TOTAL_TIME / NEON_COUNT * HMI_FRAME_RATE);
        }
    } neon_source;
    
    // Источник данных подсветки
    class led_source_t : public led_model_t::source_t
    {
        // Данные цветности простоя
        led_data_t data_idle;
        // Данные цветности маркра
        led_data_t data_marker;
        // Данные цветности готовности
        led_data_t data_success;
        
        // Прескалер фазы
        uint32_t frame;
        // Текущий разряд
        hmi_rank_t rank;
        // Контроллер плавного изменения
        led_model_t::smoother_t smoother;
        
        // Запуск эффекта разряда
        void rank_start(hmi_rank_t rank)
        {
            this->rank = rank;
            smoother.start(rank, out_get(rank));
        }
        
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_t::attached();
            
            // Рандомизация цветов
            {
                const auto hue = (hmi_sat_t)random_range_get(HMI_SAT_MIN, HMI_SAT_MAX);
                auto hsv = hmi_hsv_init(hue, HMI_SAT_MAX, HMI_SAT_MAX);
                
                // Дельта оттенка
                constexpr const hmi_sat_t HUE_DELTA = HMI_SAT_MAX / 3;
                
                // Расчет цветов
                data_idle = hsv.to_rgb();
                hsv.h += HUE_DELTA;
                data_marker = hsv.to_rgb();
                hsv.h += HUE_DELTA;
                data_success = hsv.to_rgb();
            }
            
            // Сброс данных разрядов в начальное состояние
            smoother.stop();
            for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                out_set(i, data_idle);
            
            // Запуск первого разряда
            rank_start(0);
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            source_t::refresh();
            
            // Разряд готовности
            if (rank > 0)
            {
                const auto prev_rank = rank - 1;
                if (smoother.process_needed(prev_rank))
                    out_set(prev_rank, smoother.process(prev_rank, data_success));
            }
            
            // Разряд маркера
            if (smoother.process_needed(rank))
                out_set(rank, smoother.process(rank, data_marker));

            // Прескалер фазы
            constexpr const uint32_t PHASE_FRAME_COUNT = TOTAL_TIME / LED_COUNT * HMI_FRAME_RATE;
            if (++frame < PHASE_FRAME_COUNT)
                return;
            frame = 0;
            
            // Следующий разряд
            if (rank >= LED_COUNT - 1)
                return;
            
            // Запуск на обоих разрядах
            rank_start(rank);
            rank_start(rank + 1);
        }
    public:
        // Конструктор по умолчанию
        led_source_t(void)
        {
            // Настройка плавности
            smoother.frame_count_set(HMI_SMOOTH_FRAME_COUNT * 2);
        }
    } led_source;
    
    // Структура настроек
    struct settings_t
    {
        // Час срабатывания
        uint8_t hour;
        // Маска дней недели
        uint8_t week_days;
        
        // Проверка полей
        bool check(void) const
        {
            return hour <= datetime_t::HOUR_MAX && 
                   week_days <= 0x7F;
        }
    };
    
    // Признак ожидания вывода сцены
    bool pending = false;
    // Минута вывода
    uint8_t output_minute;
    // Текущий день недели
    uint8_t week_day = datetime_t::WDAY_COUNT;
    
    // Текущие настройки
    static settings_t settings;
    
    // Фильтр плавной смены цифр на лампах
    nixie_switcher_t nixie_switcher;
    // Настройки фильтра плавной смены цифр на лампах
    nixie_switcher_t::settings_t nixie_switcher_settings;
    
    // Команда запрос настроек
    class command_settings_get_t : public ipc_command_get_t<settings_t>
    {
    public:
        // Конструктор по умолчанию
        command_settings_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_HEAT_SETTINGS_GET)
        { }
    };

    // Команда установки настроек
    class command_settings_set_t : public ipc_command_set_t<settings_t>
    {
    public:
        // Конструктор по умолчанию
        command_settings_set_t(void) : ipc_command_set_t(IPC_OPCODE_STM_HEAT_SETTINGS_SET)
        { }
    };
    
    // Команда запуска прогрева ламп
    class command_launch_now_t : public ipc_command_t
    {
    public:
        // Конструктор по умолчанию
        command_launch_now_t(void) : ipc_command_t(IPC_OPCODE_STM_HEAT_LAUNCH_NOW)
        { }
    };
    
    // Обработчик команды чтения настроек
    class settings_getter_t : public ipc_responder_t
    {
        // Комадна
        command_settings_get_t command;
    protected:
        // Получает ссылку на команду
        virtual ipc_command_t &command_get(void) override final
        {
            return command;
        }
        
        // Событие обработки данных
        virtual void work(bool idle) override final
        {
            if (idle)
                return;
            
            // Заполняем ответ
            command.response = settings;
            // Передача ответа
            transmit();
        }
    } settings_getter;
    
    // Обработчик команды записи настроек
    class settings_setter_t : public ipc_responder_t
    {
        // Сцена
        display_scene_heat_t &scene;
        // Комадна
        command_settings_set_t command;
    protected:
        // Получает ссылку на команду
        virtual ipc_command_t &command_get(void) override final
        {
            return command;
        }
        
        // Событие обработки данных
        virtual void work(bool idle) override final
        {
            if (idle)
                return;
            
            // Применение настроек
            settings = command.request;
            storage_modified();

            // Сброс срабатывания
            scene.pending = false;
            scene.week_day = datetime_t::WDAY_COUNT;
            
            // Передача ответа
            transmit();
        }
        
    public:
        // Конструктор по умолчанию
        settings_setter_t(display_scene_heat_t &_scene) : scene(_scene)
        { }
    } settings_setter;
    
    // Обработчик команды запуска прогрева ламп
    class launch_now_t : public ipc_responder_t
    {
        // Сцена
        display_scene_heat_t &scene;
        // Комадна
        command_launch_now_t command;
    protected:
        // Получает ссылку на команду
        virtual ipc_command_t &command_get(void) override final
        {
            return command;
        }
        
        // Событие обработки данных
        virtual void work(bool idle) override final
        {
            if (idle)
                return;
            
            // Запрос на показ если сцена не активна
            if (screen.scene_get() != &scene)
                scene.pending = true;
            
            // Передача ответа
            transmit();
        }
    public:
        // Конструктор по умолчанию
        launch_now_t(display_scene_heat_t &_scene) : scene(_scene)
        { }
    } launch_now;
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        return pending;
    }

    // Событие активации сцены на дисплее
    virtual void activated(void) override final
    {
        // Базовый метод
        screen_scene_t::activated();
        
        // Максимальная яркость
        light_setup_maximum(true);
        // Сброс признака ожидания вывода сцены
        pending = false;
    }

    // Событие деактивации сцены на дисплее
    virtual void deactivated(void) override final
    {
        // Базовый метод
        screen_scene_t::deactivated();
        
        // Обычная яркость
        light_setup_maximum(false);
    }
    
public:
    // Конструктор по умолчанию
    display_scene_heat_t(void) 
        : nixie_switcher(nixie_switcher_settings), 
          settings_setter(*this),
          launch_now(*this)
    {
        // Лампы
        nixie.attach(nixie_source);
        nixie.attach(nixie_switcher);
        nixie_switcher_settings.effect = nixie_switcher_t::EFFECT_SMOOTH_DEF;
        // Неонки
        neon.attach(neon_source);
        // Светодиоды
        led.attach(led_source);
    }
    
    // Обработчик секундного события (вызывается всегда)
    void second_always(void)
    {
        // Если наступил новый день
        if (week_day != rtc_week_day)
        {
            // Расчет случайной минуты
            week_day = rtc_week_day;
            output_minute = random_get(datetime_t::MINUTE_MAX);
        }
        
        // Если в текущих сутках вывод уже был
        if (output_minute == datetime_t::MINUTES_PER_HOUR)
            return;
        
        // Если не сходится час вывода
        if (settings.hour != rtc_time.hour)
            return;
        
        // Если текущий день недели не активен
        if ((settings.week_days & MASK_8(1, week_day)) == 0)
            return;
        
        // Если случайная минута еще не наступила
        if (output_minute < rtc_time.minute)
            return;
        
        // Запрос на вывод сцены
        output_minute = datetime_t::MINUTES_PER_HOUR;
        pending = true;
    }

    // Установка обработчиков команд
    void setup(void)
    {
        esp_handler_add(launch_now);
        esp_handler_add(settings_getter);
        esp_handler_add(settings_setter);
    }
} display_scene_heat;

// Текущие настройки
display_scene_heat_t::settings_t display_scene_heat_t::settings @ STORAGE_SECTION =
{
    // 14:00 - 15:00
    .hour = 14,
    // Понедельник - пятница
    .week_days = 0x1F,
};

// Информация о разрядах
const display_scene_heat_t::rank_t display_scene_heat_t::RANK_INFO[] = { 2, 0, 6, 0, 6, 0 };

// Класс сцены отображения IP адреса сети
template <typename COMMAND_GET, typename COMMAND_SET>
class display_scene_network_t : public display_scene_timeout_t<display_settings_timeout_t, COMMAND_GET, COMMAND_SET>
{
    // Базовый класс
    using base_t = display_scene_timeout_t<display_settings_timeout_t, COMMAND_GET, COMMAND_SET>;
    
    // Управление лампами
    class nixie_source_t : public nixie_number_source_t
    {
    public:
        // IP адрес
        wifi_ip_t ip;
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        { 
            // Базовый метод
            source_t::attached();

            // Предпоследний октет
            out(0, 3, ip.o[2]);
            // Последний октет
            out(3, 3, ip.o[3], true);
        }
    } nixie_source;
    
public:
    // Конструктор по умолчанию
    display_scene_network_t(display_settings_timeout_t &_settings)
        : base_t(_settings)
    {
        // Лампы
        base_t::nixie.attach(nixie_source);
    }
    
    // Запрос на показ
    void show(const wifi_ip_t &ip)
    {
        nixie_source.ip = ip;
        base_t::show_request();
    }
};

// Настройки сцены своей сети
static display_settings_timeout_t display_scene_onet_settings @ STORAGE_SECTION =
{
    .base =
    {
        .base =
        {
            .led =
            {
                .effect = led_source_t::EFFECT_NONE,
                .smooth = 4,
                .source = led_source_t::DATA_SOURCE_CUR_RANDOM,
                .rgb =
                {
                    hmi_rgb_init(255, 255, 255),
                    hmi_rgb_init(255, 255, 255),
                    hmi_rgb_init(255, 255, 255),
                    hmi_rgb_init(255, 0, 0),
                    hmi_rgb_init(255, 0, 0),
                    hmi_rgb_init(255, 0, 0),
                },
            },
            
            .neon =
            {
                .mask = 0x05,
                .period = 0,
                .smooth = 3,
                .inversion = false,
            },
            
            .nixie =
            {
                .effect = nixie_switcher_t::EFFECT_SMOOTH_SUB,
            },
        },
        .allow = true,
    },
    .timeout = 5,
};

// Сцена своей сети
static display_scene_network_t<display_command_onet_get_t, display_command_onet_set_t> 
    display_scene_onet(display_scene_onet_settings);

// Настройки сцены своей сети
static display_settings_timeout_t display_scene_cnet_settings @ STORAGE_SECTION =
{
    .base =
    {
        .base =
        {
            .led =
            {
                .effect = led_source_t::EFFECT_NONE,
                .smooth = 4,
                .source = led_source_t::DATA_SOURCE_CUR_RANDOM,
                .rgb =
                {
                    hmi_rgb_init(255, 255, 255),
                    hmi_rgb_init(255, 255, 255),
                    hmi_rgb_init(255, 255, 255),
                    hmi_rgb_init(0, 0, 255),
                    hmi_rgb_init(0, 0, 255),
                    hmi_rgb_init(0, 0, 255),
                },
            },
            
            .neon =
            {
                .mask = 0x0A,
                .period = 0,
                .smooth = 3,
                .inversion = false,
            },
            
            .nixie =
            {
                .effect = nixie_switcher_t::EFFECT_SMOOTH_SUB,
            },
        },
        .allow = true,
    },
    .timeout = 5,
};

// Сцена подключенной сети
static display_scene_network_t<display_command_cnet_get_t, display_command_cnet_set_t> 
    display_scene_cnet(display_scene_cnet_settings);

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
                out_set(i, data);
        }
    public:
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
                out_set(i, data);
        }
    public:
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
                        out_set(i, HMI_COLOR_RGB_BLACK);
                    break;
                // Появление красного в центре
                case 1:
                    out_set(0, HMI_COLOR_RGB_GREEN);
                    out_set(5, HMI_COLOR_RGB_GREEN);
                    out_set(1, HMI_COLOR_RGB_BLUE);
                    out_set(4, HMI_COLOR_RGB_BLUE);
                    out_set(2, HMI_COLOR_RGB_RED);
                    out_set(3, HMI_COLOR_RGB_RED);
                    break;
                // Первый сдвиг
                case 2:
                    out_set(0, HMI_COLOR_RGB_BLUE);
                    out_set(5, HMI_COLOR_RGB_BLUE);
                    out_set(1, HMI_COLOR_RGB_RED);
                    out_set(4, HMI_COLOR_RGB_RED);
                    out_set(2, HMI_COLOR_RGB_GREEN);
                    out_set(3, HMI_COLOR_RGB_GREEN);
                    break;
                case 3:
                    out_set(0, HMI_COLOR_RGB_RED);
                    out_set(5, HMI_COLOR_RGB_RED);
                    out_set(1, HMI_COLOR_RGB_GREEN);
                    out_set(4, HMI_COLOR_RGB_GREEN);
                    out_set(2, HMI_COLOR_RGB_BLUE);
                    out_set(3, HMI_COLOR_RGB_BLUE);
                    break;
                case 4:
                    out_set(0, HMI_COLOR_RGB_RED);
                    out_set(1, HMI_COLOR_RGB_RED);
                    out_set(2, HMI_COLOR_RGB_GREEN);
                    out_set(3, HMI_COLOR_RGB_GREEN);
                    out_set(4, HMI_COLOR_RGB_BLUE);
                    out_set(5, HMI_COLOR_RGB_BLUE);
                    break;
                case 5:
                    out_set(0, HMI_COLOR_RGB_GREEN);
                    out_set(1, HMI_COLOR_RGB_GREEN);
                    out_set(2, HMI_COLOR_RGB_BLUE);
                    out_set(3, HMI_COLOR_RGB_BLUE);
                    out_set(4, HMI_COLOR_RGB_RED);
                    out_set(5, HMI_COLOR_RGB_RED);
                    break;
                case 6:
                    out_set(0, HMI_COLOR_RGB_BLUE);
                    out_set(1, HMI_COLOR_RGB_BLUE);
                    out_set(2, HMI_COLOR_RGB_RED);
                    out_set(3, HMI_COLOR_RGB_RED);
                    out_set(4, HMI_COLOR_RGB_GREEN);
                    out_set(5, HMI_COLOR_RGB_GREEN);
                    break;
                case 7:
                    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                        out_set(i, HMI_COLOR_RGB_WHITE);
                    break;
                default:
                    assert(false);
                    break;
            }
        }
    public:
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
    // Фильтр смены цвета светодиодов
    //display_led_smooth_filter_t led_smooth;
    // Фильтр смены состояния неонок
    //display_neon_smooth_filter_t neon_smooth;
    // Фильтр смены цифр в самом начале
    //display_nixie_smooth_filter_t nixie_smooth_start;
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        return !shown;
    }
    
    // Событие активации сцены на дисплее
    virtual void activated(void) override final
    {
        // Базовый метод
        screen_scene_t::activated();
        
        // Выставление фильтров
        //nixie.attach(nixie_smooth_start);
        // Указываем, что тест уже выводили
        shown = true;
    }
    
    // Обновление сцены
    virtual void refresh(void) override final
    {
        // Базовый метод
        screen_scene_t::refresh();
        
        // Обработка фрейма
        //frame++;
        // Если наступило пол секунды
        //if (frame.half_seconds_period_get() != 0)
        //    return;
        // Номер секунды
        //auto half_second = frame.half_seconds_get();
        
        // Обработка первой секунды
        //if (half_second < 1)
        //    return;
        
        //if (half_second == 1)
        //{
        //    // Убираем эффект появления
        //    nixie.detach(nixie_smooth_start);
        //    // Разрешаем вывод неонок
        //    neon_source.allow();
        //    return;
        //}
        
        // Переход к следующей цифре
        nixie_source.next();
        // Начинаем менять подсветку со второй секунды
        //if (half_second == 2)
        //    return;
        
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
        //neon.attach(neon_smooth);
        // Светодиоды
        led.attach(led_source);
        //led.attach(led_smooth);
        
        //led_smooth.smoother.frame_count_set(HMI_FRAME_RATE / 4);
    }
} display_scene_test;

// Установка сцены по умолчанию
static void display_scene_set_default(void)
{
    // Список известных сцен
    static display_scene_t * const SCENES[] =
    {
        // Тестирование
        //&display_scene_test,
        // Репортирование о смене IP
        //&display_scene_ip_report,
        // Своя сеть
        &display_scene_onet,
        // Подключенная сеть
        &display_scene_cnet,
        // Прогрев ламп
        &display_scene_heat,
        // Дата
        &display_scene_date,
        // Время
        &display_scene_time,
    };
    
    // Обход известных сцен
    for (uint8_t i = 0; i < array_length(SCENES); i++)
        if (SCENES[i]->show_required())
        {
            screen.scene_set(SCENES[i]);
            return;
        }
    
    // Такое недопустимо
    assert(false);
}

// Событие наступления секунды
static list_handler_item_t display_second_event([](void)
{
    display_scene_heat.second_always();
    display_scene_date.second_always();
});

void display_init(void)
{
    // Инициализация сцен
    display_scene_time.setup();
    display_scene_heat.setup();
    display_scene_date.setup();
    display_scene_onet.setup();
    display_scene_cnet.setup();

    // Установка сцены по умолчанию
    display_scene_set_default();
    
    // Добавление обработчика секундного события
    rtc_second_event_add(display_second_event);
}

void display_show_ip(wifi_intf_t intf, const wifi_ip_t &ip)
{
    switch (intf)
    {
        case WIFI_INTF_STATION:
            display_scene_cnet.show(ip);
            break;
        
        case WIFI_INTF_SOFTAP:
            display_scene_onet.show(ip);
            break;
            
        default:
            assert(false);
    }
}
