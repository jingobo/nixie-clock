﻿#include "esp.h"
#include "rtc.h"
#include "timer.h"
#include "xmath.h"
#include "random.h"
#include "screen.h"
#include "storage.h"
#include "display.h"
#include "proto/display.inc.h"

// Настройки сцены времени
static display_settings_t display_settings_time @ STORAGE_SECTION =
{
    .led =
    {
        .effect = led_source_t::EFFECT_FLASH,
        .smooth = 20,
        .source = led_source_t::DATA_SOURCE_ANY_RANDOM,
        .rgb =
        {
            HMI_RGB_INIT(255, 0, 0),
            HMI_RGB_INIT(255, 255, 0),
            HMI_RGB_INIT(0, 255, 0),
            HMI_RGB_INIT(0, 255, 255),
            HMI_RGB_INIT(0, 0, 255),
            HMI_RGB_INIT(255, 0, 255),
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

// Класс базового источника для ламп
class display_nixie_datetime_source_t : public nixie_model_t::source_t
{
    // Признак вывода данных
    bool updated = false;
protected:
    // Смена части (час, минута или секунда)
    void out(hmi_rank_t index, uint8_t value)
    {
        out_set(index + 0, nixie_data_t(HMI_SAT_MAX, value / 10));
        out_set(index + 1, nixie_data_t(HMI_SAT_MAX, value % 10));
    }
    
    // Обработчик обновления разрядов
    virtual void update(void) = 0;
    
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
        update();
    }
public:
    // Секундное событие
    void second(void)
    {
        updated = false;
    }
};

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
class display_scene_custom_t : public display_scene_t
{
    // Обработчик команды чтения
    class getter_t : public ipc_responder_t
    {
        // Комадна
        COMMAND_GET command;
        // Сцена
        display_scene_custom_t &scene;
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
        getter_t(display_scene_custom_t &_scene) : scene(_scene)
        { }
    } getter;
    
    // Обработчик команды записи
    class setter_t : public ipc_responder_t
    {
        // Комадна
        COMMAND_SET command;
        // Сцена
        display_scene_custom_t &scene;
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
            scene.settings = command.request;
            scene.settings_changed();
            storage_modified();
            
            // Передача ответа
            transmit();
        }
        
    public:
        // Конструктор по умолчанию
        setter_t(display_scene_custom_t &_scene) : scene(_scene)
        { }
    } setter;
    
    // Ссылка на настройки
    SETTINGS &settings;
    
    // Источник светодиодов
    led_source_t led_source;
    // Источник неонок
    neon_source_t neon_source;
    // Фильтр плавной смены цифр на лампах
    nixie_switcher_t nixie_switcher;
    
protected:
    // Получает ссылку на типизированные настройки
    display_settings_t& settings_get(void) const
    {
        return (display_settings_t&)settings;
    }

    // Обработчик примнения настроек
    virtual void settings_apply(void)
    {
        // TODO: сортировка
        led_source.settings_apply();
        neon_source.settings_apply();
    }
    
    // Обработчик изменения настроек
    virtual void settings_changed(void)
    {
        settings_apply();
    }
    
    // Секундное событие
    virtual void second(void) override
    {
        // Базовый метод
        screen_scene_t::second();
        // Оповещение источников
        neon_source.second();
        // Пробуем установить другую сцену
        display_scene_set_default();
    }
public:
    // Конструктор по умолчанию
    display_scene_custom_t(SETTINGS &_settings) 
        : settings(_settings), setter(*this), getter(*this), led_source(_settings.led),
          neon_source(_settings.neon), nixie_switcher(_settings.nixie)
    {
        // Источники
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

// Сцена отображения времени
class display_scene_time_t : public display_scene_custom_t<display_settings_t, display_command_time_get_t, display_command_time_set_t>
{
    // Управление лампами
    class nixie_source_t : public display_nixie_datetime_source_t
    {
    protected:
        // Обработчик обновления разрядов
        virtual void update(void) override final
        {
            // Часы
            out(0, rtc_time.hour);
            // Минуты
            out(2, rtc_time.minute);
            // Секунды
            out(4, rtc_time.second);
        }
    } nixie_source;
    
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        return true;
    }
    
    // Секундное событие
    virtual void second(void) override final
    {
        // Базовый метод
        display_scene_custom_t::second();
        // Оповещение источников
        nixie_source.second();
    }
public:
    // Конструктор по умолчанию
    display_scene_time_t(void) : display_scene_custom_t(display_settings_time)
    {
        // Лампы
        nixie.attach(nixie_source);
        
        // Финальное применение настроек
        settings_apply();
    }
} display_scene_time;

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
    
    // Событие установки сцены на дисплей
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

// Сцена вывод IP адреса
static class display_scene_ip_report_t : public display_scene_t
{
    // Источник данных для лмап
    class nixie_source_t : public nixie_model_t::source_t
    {
        // Установка октета
        void octet_set(hmi_rank_t rank, uint8_t value)
        {
            out_set(rank + 0, nixie_data_t(HMI_SAT_MAX, value / 100 % 10, true));
            out_set(rank + 1, nixie_data_t(HMI_SAT_MAX, value / 10 % 10));
            out_set(rank + 2, nixie_data_t(HMI_SAT_MAX, value / 1 % 10));
        }
    public:
        // Установка адреса
        void address_set(const wifi_ip_t &value)
        {
            octet_set(0, value.o[2]);
            octet_set(3, value.o[3]);
        }
    } nixie_source;
    
    // Источник данных для неонок
    class neon_source_t : public neon_model_t::source_t
    {
    public:
        // Установка интерфейса
        void intf_set(wifi_intf_t intf)
        {
            bool station;
            switch (intf)
            {
                case WIFI_INTF_STATION:
                    station = true;
                    break;
                    
                case WIFI_INTF_SOFTAP:
                    station = false;
                    break;
                    
                default:
                    assert(false);
                    return;
            }
            
            out_set(0, neon_data_t(station ? HMI_SAT_MIN : HMI_SAT_MAX));
            out_set(2, neon_data_t(station ? HMI_SAT_MIN : HMI_SAT_MAX));
            
            out_set(1, neon_data_t(station ? HMI_SAT_MAX : HMI_SAT_MIN));
            out_set(3, neon_data_t(station ? HMI_SAT_MAX : HMI_SAT_MIN));
        }
    } neon_source;
    
    // Источник данных для светодиодов
    class led_source_t : public led_model_t::source_t
    {
    public:
        // Установка интерфейса
        void intf_set(wifi_intf_t intf)
        {
            bool station;
            switch (intf)
            {
                case WIFI_INTF_STATION:
                    station = true;
                    break;
                    
                case WIFI_INTF_SOFTAP:
                    station = false;
                    break;
                    
                default:
                    assert(false);
                    return;
            }
            
            for (hmi_rank_t i = 0; i < LED_COUNT; i++)
                out_set(i,  station ? HMI_COLOR_RGB_RED : HMI_COLOR_RGB_GREEN);
        }
    } led_source;
    
    // Данные по интерфейсам
    struct
    {
        // IP адрес
        wifi_ip_t ip;
        // Признак показа
        bool show = false;
    } intf[WIFI_INTF_COUNT];
    // Фильтр смены цвета светодиодов
    //display_led_smooth_filter_t led_smooth;
protected:
    // Получает, нужно ли отобразить сцену
    virtual bool show_required(void) override final
    {
        for (auto i = 0; i < WIFI_INTF_COUNT; i++)
            if (intf[i].show)
                return true;
        
        return false;
    }
    
    // Событие установки сцены на дисплей
    virtual void activated(void) override final
    {
        // Базовый метод
        screen_scene_t::activated();

        // Сброс номера фрймов
        //frame.reset();
        
        for (auto i = 0; i < WIFI_INTF_COUNT; i++)
            if (intf[i].show)
            {
                intf[i].show = false;
                
                // Неонки
                nixie_source.address_set(intf[i].ip);
                neon_source.intf_set((wifi_intf_t)i);
                led_source.intf_set((wifi_intf_t)i);
                return;
            }
        
        assert(false);
    }
    
    // Обновление сцены
    virtual void refresh(void) override final
    {
        // Базовый метод
        screen_scene_t::refresh();
        
        // Обработка фрейма
        //frame++;
        
        // Задержка показа
        //if (frame.seconds_get() < 3)
        //    return;
        
        // Активация если есть что показывать
        if (show_required())
        {
            activated();
            return;
        }
        
        // Конец сцены
        display_scene_set_default();
    }
public:
    // Конструктор по умолчанию
    display_scene_ip_report_t(void)
    {
        // Лампы
        nixie.attach(nixie_source);
        // Неонки
        neon.attach(neon_source);
        //neon.attach(neon_smooth);
        // Светодиоды
        led.attach(led_source);
        //led.attach(led_smooth);
    }
    
    // Запрос на показ
    void show(wifi_intf_t i, const wifi_ip_t &ip)
    {
        intf[i].ip = ip;
        intf[i].show = true;
    }
} display_scene_ip_report;

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
        // Основные часы
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

void display_init(void)
{
    display_scene_set_default();
    display_scene_time.setup();
}

void display_show_ip(const wifi_intf_t &intf, wifi_ip_t ip)
{
    display_scene_ip_report.show(intf, ip);
}
