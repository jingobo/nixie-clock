#include "esp.h"
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
            nixie_number_source_t::attached();
            
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
            nixie_number_source_t::attached();

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
    static constexpr const uint8_t TOTAL_TIME = 240;
    
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
            nixie_number_source_t::attached();

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
        if (nixie_source.ip == ip)
            return;
        
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
    class nixie_source_t : public nixie_model_t::source_smoother_def_t
    {
        // Текущая выводимая цифра
        uint8_t digit;
        
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_smoother_def_t::attached();
            
            // Начинаем с нулевой цифры
            digit = 0;
            
            // Сначала плавно
            const auto to = nixie_data_t(HMI_SAT_MAX, digit, true);
            const auto from = nixie_data_t(HMI_SAT_MIN, digit, true);
            for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
                smoother.start(i, from, to);
        }
        
    public:
        // Переход к следующей цифре
        void next(void)
        {
            if (digit >= 9)
                return;
            digit++;
            
            // Установка всех разрядов текущей цифрой
            const auto data = nixie_data_t(HMI_SAT_MAX, digit, true);
            for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
                out_set(i, data);
        }
    } nixie_source;
    
    // Источник данных для неонок
    class neon_source_t : public neon_model_t::source_smoother_def_t
    {
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_smoother_def_t::attached();
            
            // Сброс состояний
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                out_set(i, HMI_SAT_MIN);
        }
        
    public:
        // Разрешение вывода неонок
        void allow(void)
        {
            // Установка состояний
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                smoother.start(i, HMI_SAT_MIN, HMI_SAT_MAX);
        }
    } neon_source;
    
    // Источник данных для светодиодов
    class led_source_t : public led_model_t::source_smoother_def_t
    {
        // Текущая стадия
        uint8_t stage;
        
        // Устанавливает цвет разряда
        void rank_set(hmi_rank_t index, hmi_rgb_t color)
        {
             smoother.start(index, out_get(index), color);
        }
        
        // Обнволение с указанием текущей стадии
        void update(uint8_t value)
        {
            stage = value;
            
            // Относительно стадии указываем цвет
            switch (stage)
            {
                // Всё обнулить
                case 0:
                case 8:
                    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                        rank_set(i, HMI_COLOR_RGB_BLACK);
                    break;
                    
                // Появление красного в центре
                case 1:
                    rank_set(0, HMI_COLOR_RGB_GREEN);
                    rank_set(5, HMI_COLOR_RGB_GREEN);
                    rank_set(1, HMI_COLOR_RGB_BLUE);
                    rank_set(4, HMI_COLOR_RGB_BLUE);
                    rank_set(2, HMI_COLOR_RGB_RED);
                    rank_set(3, HMI_COLOR_RGB_RED);
                    break;
                    
                // Первый сдвиг
                case 2:
                    rank_set(0, HMI_COLOR_RGB_BLUE);
                    rank_set(5, HMI_COLOR_RGB_BLUE);
                    rank_set(1, HMI_COLOR_RGB_RED);
                    rank_set(4, HMI_COLOR_RGB_RED);
                    rank_set(2, HMI_COLOR_RGB_GREEN);
                    rank_set(3, HMI_COLOR_RGB_GREEN);
                    break;
                    
                case 3:
                    rank_set(0, HMI_COLOR_RGB_RED);
                    rank_set(5, HMI_COLOR_RGB_RED);
                    rank_set(1, HMI_COLOR_RGB_GREEN);
                    rank_set(4, HMI_COLOR_RGB_GREEN);
                    rank_set(2, HMI_COLOR_RGB_BLUE);
                    rank_set(3, HMI_COLOR_RGB_BLUE);
                    break;
                    
                case 4:
                    rank_set(0, HMI_COLOR_RGB_RED);
                    rank_set(1, HMI_COLOR_RGB_RED);
                    rank_set(2, HMI_COLOR_RGB_GREEN);
                    rank_set(3, HMI_COLOR_RGB_GREEN);
                    rank_set(4, HMI_COLOR_RGB_BLUE);
                    rank_set(5, HMI_COLOR_RGB_BLUE);
                    break;
                    
                case 5:
                    rank_set(0, HMI_COLOR_RGB_GREEN);
                    rank_set(1, HMI_COLOR_RGB_GREEN);
                    rank_set(2, HMI_COLOR_RGB_BLUE);
                    rank_set(3, HMI_COLOR_RGB_BLUE);
                    rank_set(4, HMI_COLOR_RGB_RED);
                    rank_set(5, HMI_COLOR_RGB_RED);
                    break;
                    
                case 6:
                    rank_set(0, HMI_COLOR_RGB_BLUE);
                    rank_set(1, HMI_COLOR_RGB_BLUE);
                    rank_set(2, HMI_COLOR_RGB_RED);
                    rank_set(3, HMI_COLOR_RGB_RED);
                    rank_set(4, HMI_COLOR_RGB_GREEN);
                    rank_set(5, HMI_COLOR_RGB_GREEN);
                    break;
                    
                case 7:
                    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                        rank_set(i, HMI_COLOR_RGB_WHITE);
                    break;
                    
                default:
                    assert(false);
                    break;
            }
        }
        
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_smoother_def_t::attached();
            // Сброс состояний
            update(0);
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
    
    // Счетчик фреймов
    uint32_t frame;
    // Указывает, был ли показан тест
    bool shown = false;
    
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
        
        // Сброс счетчика
        frame = 0;
        // Максимальная яркость
        light_setup_maximum(true);
    }
    
    // Событие деактивации сцены на дисплее
    virtual void deactivated(void) override final
    {
        // Базовый метод
        screen_scene_t::deactivated();
        
        // Обычная яркость
        light_setup_maximum(false);
    }
    
    // Обновление сцены
    virtual void refresh(void) override final
    {
        // Базовый метод
        screen_scene_t::refresh();
        
        // Количество фреймов на половину секунды
        constexpr const auto HALF_SECOND_FRAME_COUNT = HMI_FRAME_RATE / 2;
        
        // Прескалер на половину секунды
        frame++;
        if (frame % HALF_SECOND_FRAME_COUNT != 0)
            return;
        
        // Номер секунды
        auto half_second = frame / HALF_SECOND_FRAME_COUNT;
        
        // Обработка первой секунды
        if (half_second < 1)
            return;
        
        if (half_second == 1)
        {
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
        shown = true;
        display_scene_set_default();
    }
    
public:
    // Конструктор по умолчанию
    display_scene_test_t(void)
    {
        nixie.attach(nixie_source);
        neon.attach(neon_source);
        led.attach(led_source);
    }
} display_scene_test;

// Сцена наступления нового года
static class display_scene_year_t : public display_scene_t
{
    // Управление лампами
    class nixie_source_t : public nixie_number_source_t
    {
        // Смещение надписи
        uint8_t offset;
        // Направление
        bool to_right;
        // Прескалер фреймов
        uint32_t frame;
        // Контроллер плавного изменения
        nixie_model_t::smoother_to_t smoother;
        
        // Очищает разряд
        void clear_rank(hmi_rank_t index)
        {
            out_set(index, nixie_data_t());
        }

        // Плавно гасит разряд
        void fade_rank(hmi_rank_t index)
        {
            auto data = out_get(index);
            data.sat = HMI_SAT_MIN;
            nixie_number_source_t::smoother_start(smoother, index, data);
        }
        
        // Обновление разрядов
        void update_ranks(void)
        {
            // Год
            out(offset, 4, datetime_t::YEAR_BASE + rtc_time.year);
            
            switch (offset)
            {
                case 0:
                    fade_rank(4);
                    clear_rank(5);
                    break;
                    
                case 1:
                    fade_rank(0);
                    fade_rank(5);
                    break;
                    
                case 2:
                    clear_rank(0);
                    fade_rank(1);
                    break;
                    
                default:
                    assert(false);
            }
        }
        
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        { 
            // Базовый метод
            nixie_number_source_t::attached();

            // Начальные данные
            offset = 1;
            to_right = true;
            smoother.stop();
            update_ranks();

            // В первый раз 10 секунд
            frame = HMI_FRAME_RATE * 10;
        }
        
        // Обновление данных
        virtual void refresh(void) override final
        {
            // Базовый метод
            nixie_number_source_t::refresh();
            
            // Обработка плавности
            nixie_number_source_t::smoother_process(smoother);
            
            // Обработка прескалера
            if (frame-- > 0)
                return;
            frame = HMI_FRAME_RATE * 5;
            
            // Обработка направления
            if (to_right)
                offset++;
            else
                offset--;
            if (offset != 1)
                to_right = !to_right;
            
            // Обнвление разрядов
            update_ranks();
        }
        
    public:
        // Конструктор по умолчанию
        nixie_source_t(void)
        {
            smoother.frame_count_set(HMI_SMOOTH_FRAME_COUNT);
        }
    } nixie_source;
    
    // Источник данных для неонок
    class neon_source_t : public neon_model_t::source_t
    {
    protected:
        // Событие присоединения к цепочке
        virtual void attached(void) override final
        {
            // Базовый метод
            source_t::attached();
            
            // Сброс состояний
            for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
                out_set(i, HMI_SAT_MIN);
        }
    } neon_source;

    // Источник данных для светодиодов
    class led_source_t : public led_model_t::source_animation_t
    {
        // Предыдущий выбранный оттенок
        hmi_sat_t hue_last = 0;
    protected:
        // Обработчик заполнения данных ключевого кадра
        virtual void key_frame_fill(uint32_t &index) override final
        {
            const auto SNOW_COLOR = hmi_rgb_init(127, 127, 255);
            
            switch (index)
            {
                case 0:
                    smoother.frame_count_set(80);
                    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                        smoother_start(i, led_data_t());
                    delay_frame_count = HMI_SMOOTH_FRAME_COUNT;
                    return;
                    
                case 1:
                    smoother.frame_count_set(90);
                    smoother_start(0, HMI_COLOR_RGB_RED);
                    smoother_start(5, HMI_COLOR_RGB_GREEN);
                    return;
                    
                case 2:
                    smoother_start(1, HMI_COLOR_RGB_RED);
                    smoother_start(4, HMI_COLOR_RGB_GREEN);
                    return;

                case 3:
                    smoother.frame_count_set(10);
                    smoother_start(2, hmi_rgb_init(255, 127, 127));
                    smoother_start(3, hmi_rgb_init(127, 255, 127));
                    return;

                case 4:
                    smoother_start(2, HMI_COLOR_RGB_WHITE);
                    smoother_start(3, HMI_COLOR_RGB_WHITE);
                    return;

                case 5:
                    smoother.frame_count_set(20);
                    smoother_start(1, HMI_COLOR_RGB_WHITE);
                    smoother_start(4, HMI_COLOR_RGB_WHITE);
                    return;

                case 6:
                    smoother_start(0, HMI_COLOR_RGB_WHITE);
                    smoother_start(5, HMI_COLOR_RGB_WHITE);
                    delay_frame_count = 60;
                    return;
                    
                case 7:
                case 9:
                case 11:
                    smoother.frame_count_set(60);
                    smoother_start(0, HMI_COLOR_RGB_BLACK);
                    smoother_start(5, HMI_COLOR_RGB_BLACK);
                    for (hmi_rank_t i = 1; i < 5; i++)
                        smoother_start(i, SNOW_COLOR);
                    delay_frame_count = 60;
                    return;

                case 8:
                case 10:
                    smoother.frame_count_set(20);
                    for (hmi_rank_t i = 1; i < 5; i++)
                        smoother_start(i, HMI_COLOR_RGB_WHITE);
                    return;

                case 12:
                    smoother.frame_count_set(90);
                    smoother_start(1, HMI_COLOR_RGB_RED);
                    smoother_start(2, HMI_COLOR_RGB_GREEN);
                    smoother_start(3, HMI_COLOR_RGB_RED);
                    smoother_start(4, HMI_COLOR_RGB_GREEN);
                    delay_frame_count = 90;
                    return;

                case 13:
                    smoother_start(1, HMI_COLOR_RGB_GREEN);
                    smoother_start(2, HMI_COLOR_RGB_RED);
                    smoother_start(3, HMI_COLOR_RGB_GREEN);
                    smoother_start(4, HMI_COLOR_RGB_RED);
                    delay_frame_count = 90;
                    return;
                    
                default:
                    if (index % 2 == 0)
                    {
                        delay_frame_count = 160 + HMI_FRAME_RATE * 3;
                        smoother.frame_count_set(60);
                        const auto color = led_random_color_get(hue_last, 160);
                        for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                            smoother_start(i, color);
                        
                        return;
                    }
                    
                    smoother.frame_count_set(20);
                    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                        smoother_start(i, HMI_COLOR_RGB_WHITE);
                    return;
            }
        }
    } led_source;
    
    // Таймаут показа в секунда
    uint8_t show_timeout = 0;

protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        return show_timeout > 0;
    }

    // Секундное событие
    virtual void second(void) override final
    {
        // Базовый метод
        screen_scene_t::second();
        
        // Показ 1 минуту
        if (--show_timeout <= 0)
            display_scene_set_default();
    }

    // Событие активации сцены на дисплее
    virtual void activated(void) override final
    {
        // Базовый метод
        screen_scene_t::activated();
        
        // Максимальная яркость
        light_setup_maximum(true);
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
    display_scene_year_t(void)
    {
        nixie.attach(nixie_source);
        neon.attach(neon_source);
        led.attach(led_source);
    }

    // Обработчик секундного события (вызывается всегда)
    void second_always(void)
    {
        // Фильтр на нулевую секунду нового года
        if (rtc_time.year == datetime_t::YEAR_MIN ||
            rtc_time.month != datetime_t::MONTH_MIN ||
            rtc_time.day != datetime_t::DAY_MIN ||
            rtc_time.hour != datetime_t::HOUR_MIN ||
            rtc_time.minute != datetime_t::MINUTE_MIN ||
            rtc_time.second > 1)
            return;
        
        // Запрос на показ
        show_timeout = datetime_t::SECONDS_PER_MINUTE;
    }
} display_scene_year;

// Установка сцены по умолчанию
static void display_scene_set_default(void)
{
    // Список известных сцен
    static display_scene_t * const SCENES[] =
    {
        // Тестирование
        &display_scene_test,
        // Новый год
        &display_scene_year,
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
    display_scene_year.second_always();
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
