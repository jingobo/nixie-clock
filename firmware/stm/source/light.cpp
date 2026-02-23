#include "esp.h"
#include "i2c.h"
#include "debug.h"
#include "light.h"
#include "timer.h"
#include "xmath.h"
#include "system.h"
#include "screen.h"
#include "random.h"
#include "storage.h"
#include "proto/light.inc.h"

// Значение в люксах при полной яркости 
constexpr const auto LIGHT_LUX_MAX = 100.0f;

// Настройки освещенности
static light_settings_t light_settings @ STORAGE_SECTION =
{
    .gain = 0,
    .level = 80,
    .smooth = 2,
    .exposure = 4,
    .autoset = true,
    .nightmode = false,
};

// Базовый класс датчика освещенности
struct light_sensor_t
{
    // Производит детектирование
    virtual bool detect(void) const = 0;
    // Производит начальное конфигурирование
    virtual bool config(void) const = 0;
    // Подготовка к чтению (возращает время)
    virtual uint32_t measure(void) const = 0;
    // Производит чтение результата
    virtual float_t read(void) const = 0;
};

// Режим усиления для датчика TSL2591
#define LIGHT_TSL_GAIN      1

// Определение коофициента усиления
#if LIGHT_TSL_GAIN == 0
    constexpr const auto LIGHT_TSL_GAIN_F = 1.0f;
#elif LIGHT_TSL_GAIN == 1
    constexpr const auto LIGHT_TSL_GAIN_F = 25.0f;
#elif LIGHT_TSL_GAIN == 2
    constexpr const auto LIGHT_TSL_GAIN_F = 428.0f;
#elif LIGHT_TSL_GAIN == 3
    constexpr const auto LIGHT_TSL_GAIN_F = 9876.0f;
#else
    #error unknown gain mode
#endif

// Режим времени интеграции для датчика TSL2591
#define LIGHT_TSL_ATIME     3
    
// Определение коофициента усиления
#if LIGHT_TSL_ATIME == 0
    constexpr const auto LIGHT_TSL_ATIME_F = 100.0f;
#elif LIGHT_TSL_ATIME == 1
    constexpr const auto LIGHT_TSL_ATIME_F = 200.0f;
#elif LIGHT_TSL_ATIME == 2
    constexpr const auto LIGHT_TSL_ATIME_F = 300.0f;
#elif LIGHT_TSL_ATIME == 3
    constexpr const auto LIGHT_TSL_ATIME_F = 400.0f;
#elif LIGHT_TSL_ATIME == 4
    constexpr const auto LIGHT_TSL_ATIME_F = 500.0f;
#elif LIGHT_TSL_ATIME == 5
    constexpr const auto LIGHT_TSL_ATIME_F = 600.0f;
#else
    #error unknown gain mode
#endif

// Класс датчика освещенности TSL2591
static const class light_sensor_tsl_t final : public light_sensor_t
{
    // Адрес устройства
    static constexpr const uint8_t ADDRESS = 0x29;
    
    // Производит установку регистра
    static bool write(uint8_t reg)
    {
        // Старт на запись
        if (!i2c_start(ADDRESS, I2C_MODE_WRITE))
            return false;
        
        // Ожидание передачи адреса
        if (!i2c_wait_txe())
            return false;
        
        // Передача регистра
        i2c_write(0xA0 | reg);
        return true;
    }

    // Производит запись регистра
    static bool write(uint8_t reg, uint8_t arg)
    {
        if (!write(reg))
            return false;
        
        // Ожидание передачи регистра
        if (!i2c_wait_txe())
            return false;
        
        // Передача аргумента
        i2c_write(arg);
        
        // Ожидание передачи данных
        if (!i2c_wait_btf())
            return false;
        
        // Фаза стопа
        i2c_stop();
        return i2c_finalize();
    }
    
public:    
    // Производит детектирование
    virtual bool detect(void) const override
    {
        // Начальный регистр
        if (!write(0x11))
            return false;

        // Ожидание передачи регистра
        if (!i2c_wait_btf())
            return false;
        
        // Чтение регистров
        uint8_t dummy, id;
        if (!i2c_read_x2(ADDRESS, dummy, id))
            return false;
        
        // Проверка идентификатора
        return id == 0x50;
    }
    
    // Производит начальное конфигурирование
    virtual bool config(void) const override
    {
        if (!write(0x00, 0x03))                                                 // Power On
            return false;
        
        return write(0x01, LIGHT_TSL_ATIME << 0 | LIGHT_TSL_GAIN << 4);         // Interpolation/Gain
    }
    
    // Подготовка к чтению
    virtual uint32_t measure(void) const override
    {
        return XK((uint32_t)LIGHT_TSL_ATIME_F);
    }

    // Производит чтение результата
    virtual float_t read(void) const override
    {
        if (!write(0x14))
            return NAN;

        // Ожидание передачи регистра
        if (!i2c_wait_btf())
            return NAN;
        
        // Переоткрытие на чтение
        if (!i2c_start(ADDRESS, I2C_MODE_READ))
            return NAN;
        
        // Ожидание приёма
        if (!i2c_wait_rxne())
            return NAN;

        // Данные каналов
        union
        {
            uint16_t ch[2];
            uint8_t data[4];
        };
        
        // Чтение первого байта
        data[0] = i2c_read();
        
        // Ожидание приёма 2 байт
        if (!i2c_wait_btf())
            return NAN;

        // NAK N-2
        i2c_nack();
        
        // Errata
        i2c_scl_low();
        
        // Чтение второго байта
        data[1] = i2c_read();
        
        // NAK N-1
        i2c_stop();
        
        // Чтение третьего байта и запуск чтения послнего
        data[2] = i2c_read();
        
        // Errata
        i2c_scl_alt();
        
        // Ожидание приёма последнего байта
        if (!i2c_wait_rxne())
            return NAN;

        // Чтение послнего байта
        data[3] = i2c_read();
        
        // Завершение
        if (!i2c_finalize())
            return NAN;
        
        // В рандом младший бит
        random_noise_bit((data[0] & 2) != 0);
        
        // Конвертирование
        float_t lux;
        {
            // Учет переосвещенности
            if (ch[0] >= UINT16_MAX)
                return LIGHT_LUX_MAX;
            
            // Учет деления на ноль
            if (ch[0] <= 0)
                ch[0] = 1;
            
            // Перевод каналов в вещественное число
            const float_t ch0 = ch[0];
            const float_t ch1 = ch[1];
            
            // Количество люкс на отсчет
            constexpr const auto CPL = (LIGHT_TSL_ATIME_F * LIGHT_TSL_GAIN_F) / 408.0f;
            
            // Пересчет
            lux = ((ch0 - ch1)) * (1.0f - (ch1 / ch0)) / CPL;
        }
        
        return lux;
    }
} LIGHT_SENSOR_TSL;

// Класс датчика освещенности BH1750
static const class light_sensor_bh_t final : public light_sensor_t
{
    // Адрес устройства
    static constexpr const uint8_t ADDRESS = 0x23;
    
    // Производит установку команды
    static bool write(uint8_t opcode)
    {
        // Старт на запись
        if (!i2c_start(ADDRESS, I2C_MODE_WRITE))
            return false;
        
        // Ожидание передачи адреса
        if (!i2c_wait_txe())
            return false;
        
        // Передача команды
        i2c_write(opcode);
        
        // Ожидание передачи команды
        if (!i2c_wait_btf())
            return false;
        
        // Фаза стопа
        i2c_stop();
        return i2c_finalize();
    }
    
public:    
    // Производит детектирование
    virtual bool detect(void) const override
    {
        return write(0x00);
    }
    
    // Производит начальное конфигурирование
    virtual bool config(void) const override
    {
        // Нет предварительной конфигурации
        return true;
    }
    
    // // Подготовка к чтению
    virtual uint32_t measure(void) const override
    {
        if (!write(0x01))                                                       // Power On
            return 0;
        
        if (!write(0x42))                                                       // Measurement time MSB
            return 0;

        if (!write(0x65))                                                       // Measurement time LSB
            return 0;

        if (!write(0x21))                                                       // Single Hi-Res 2 
            return 0;

        // 1 секунда
        return XK(120);
    }
    
    // Производит чтение результата
    virtual float_t read(void) const override
    {
        union
        {
            uint16_t raw;
            struct
            {
                uint8_t lsb, msb;
            };
        };
        
        // Чтение
        if (!i2c_read_x2(ADDRESS, msb, lsb))
            return NAN;

        // В рандом младший бит
        random_noise_bit((lsb & 2) != 0);
        
        // Точность
        constexpr const auto ACCURACY = 1.2f;
        // Делитель при высокой точности
        constexpr const auto HIRES_DIV = 2.0f;
        // Коофициент калибровки 
        constexpr const auto CALIB_DIV = 1.5f;
        // Количество люкс на отсчет
        constexpr const auto CPL = ACCURACY * HIRES_DIV * CALIB_DIV;

        // Пересчет
        return raw / CPL;
    }
} LIGHT_SENSOR_BH;

// Список поддерживаемых датчиков
static const light_sensor_t * const LIGHT_SENSORS[]
{
    &LIGHT_SENSOR_BH,
    &LIGHT_SENSOR_TSL,
};

// Обработчик таймера автомата состояний (предварительное объявление)
static void light_state_timer_cb(void);

// Текущее состояние автомата
static __no_init enum light_state_t
{
    // Конфигурирование
    LIGHT_STATE_DETECT,
    // Конфигурирование
    LIGHT_STATE_CONFIG,
    // Запуск измерения
    LIGHT_STATE_MEASURE,
    // Чтение результатов
    LIGHT_STATE_READING,
} light_state;

// Текущий используемый датчик
static uint8_t light_sensor = array_length(LIGHT_SENSORS);

// Максимальное покзаание в люксах
static __no_init float_t light_max_lux;
// Текущее покзаание в люксах
static __no_init float_t light_current_lux;
// Коофициент усиления значения освещенности
static __no_init float_t light_gain_coef_lux;

// Текущее показание уровня освещенности
static __no_init uint8_t light_current_level;
// Время выдержки уровня освещенности
static __no_init uint32_t light_exposure_time;
// Максимальное время выдержки уровня освещенности
static __no_init uint32_t light_exposure_time_max;

// Таймер автомата состояний
static timer_t light_state_timer(light_state_timer_cb);

// Пересчет значения уровня освещенности
static void light_level_flush(void)
{
    if (isnan(light_current_lux))
    {
        light_current_level = LIGHT_LEVEL_MAX;
        return;
    }

    // Точки линеаризации
    static const math_point2d_t<float_t, uint8_t> POINTS[] =
    {
        { 0.0f,   0 },
        { 5.0f,   10 },
        { 10.0f,  24 },
        { 15.0f,  34 },
        { 20.0f,  50 },
        { 30.0f,  61 },
        { 50.0f,  77 },
        { 70.0f,  89 },
        // Ограничение
        { LIGHT_LUX_MAX, LIGHT_LEVEL_MAX },
    };

    // Интерполяция
    light_current_level = math_linear_interpolation(light_current_lux * light_gain_coef_lux, POINTS, array_length(POINTS));
}

// Обновление уровня освещенности
static void light_level_update(void)
{
    // Откладываем обновление
    light_exposure_time = mcu_tick_get();
    
    // Максимальное значение за выдержку
    light_current_lux = light_max_lux;
    light_max_lux = 0.0f;
    
    // Обновление уровня
    light_level_flush();
}

// Производит переход к указанному состоянию автомата
static void light_state_next_set(light_state_t state, uint32_t us = TIMER_US_MIN)
{
    light_state_timer.start_us(us);
    light_state = state;
}

// Обрабогтчик ошибки измерения
static void light_measure_error(void)
{
    // Переход к следующему датчику
    if (++light_sensor >= array_length(LIGHT_SENSORS))
        light_sensor = 0;
    
    // Текущее значение не известно
    light_current_lux = NAN;
    light_level_flush();
    
    // Переход к детектированию
    light_state_next_set(LIGHT_STATE_DETECT);
}

// Обработчик таймера автомата состояний
static void light_state_timer_cb(void)
{
    /* Если активна линия ESP, то откладываем
     * В Errata запрещено использование SPI1 и I2C в случае ремапа */
    if (esp_wire_active())
    {
        // Повтор через 10 мС
        light_state_timer.start_us(XK(10));
        return;
    }
    
    // Текущий датчик
    const auto &sensor = *LIGHT_SENSORS[light_sensor];
    
    // Запуск I2C
    i2c_init();
        
    switch (light_state)
    {
        case LIGHT_STATE_DETECT:
            // Детектирование
            if (!sensor.detect())
            {
                light_measure_error();
                break;
            }
            
            // Переход к конфигурированию
            light_state_next_set(LIGHT_STATE_CONFIG);
            break;
            
        case LIGHT_STATE_CONFIG:
            // Конфигурирование
            if (!sensor.config())
            {
                light_measure_error();
                break;
            }
            
            // Переход к измерению
            light_state_next_set(LIGHT_STATE_MEASURE);
            break;

        case LIGHT_STATE_MEASURE:
            {
                const auto delay = sensor.measure();
                if (delay <= 0)
                {
                    light_measure_error();
                    break;
                }
                
                // Переход к чтению
                light_state_next_set(LIGHT_STATE_READING, delay + 50);
            }
            break;
            
        case LIGHT_STATE_READING:
            {
                // Чтение
                const auto lux = sensor.read();
                if (isnan(lux))
                {
                    light_measure_error();
                    break;
                }
                
                // Отладка
                {
                    // static uint8_t packet[6]= { 0xAA, 0xBB };
                    // memcpy(packet + 2, &lux, 4);
                    // debug_write(packet, sizeof(packet));
                }
                
                // Следующее измерение
                light_state_next_set(LIGHT_STATE_MEASURE);
                
                // Если текущее значение не определенно или больше
                if (isnan(light_current_lux) || light_current_lux < lux)
                {
                    light_max_lux = lux;
                    light_level_update();
                    break;
                }
                
                // Определение минимума
                if (light_max_lux < lux)
                    light_max_lux = lux;
            }

            // Обработка выдержки
            if (mcu_tick_get() - light_exposure_time >= light_exposure_time_max)
                light_level_update();
            break;
    }
    
    // Отключение I2C
    i2c_deinit();
}

// Признак нобходимости установки максимального уровня
static uint8_t light_setup_maximum_count = 0;

// Класс управления уровнем освещенности
template <typename MODEL>
class light_control_t : public MODEL::transceiver_t
{
protected:
    // Псевдонимы
    using model_t = MODEL;
    using data_t = typename model_t::data_t;
    using base_t = typename model_t::transceiver_t;

    // Текущее значение яркости
    uint8_t level;
    // Целевое значение яркости
    uint8_t level_target;
    
    // Текущее количество фреймов
    uint32_t frame;
    // Конечное количество фреймов
    uint32_t frame_count;
    
    // Получает данные относительно освещенности
    virtual data_t data_get(data_t source, uint8_t level) const = 0;
public:
    // Получает конечный уровень освещенности
    static uint8_t level_next_get()
    {
        auto result = light_settings.level;
        if (light_setup_maximum_count > 0)
            result = LIGHT_LEVEL_MAX;
        else if (light_settings.autoset)
            result = light_current_level;
        
        return result;
    }
    
    // Сброс контроллера
    void reset(void)
    {
        level = level_target = level_next_get();
        
        // Конечное количество фреймов
        frame_count = light_settings.autoset && light_setup_maximum_count <= 0 ? 
            hmi_time_to_frame_count(light_settings.smooth) : 
            0;
        
        frame = 0;
        frame_count = maximum<uint32_t>(frame_count, 1);
    }
    
protected:
    // Получает, можно ли слой переносить в другую модель
    virtual bool moveable_get(void) const override final
    {
        // Перехватчика источника перемещать нельзя
        return false;
    }
    
    // Получает приоритет слоя
    virtual uint8_t priority_get(void) const override final
    {
        // Низший приоритет
        return model_t::PRIORITY_LIGHT;
    }
        
    // Обработчик изменения данных
    virtual void data_changed(hmi_rank_t index, data_t &data) override final
    {
        if (frame != frame_count)
            return;

        // Базовый метод
        base_t::out_set(index, data_get(data, level_target));
    }
    
    // Обновление данных
    virtual void refresh(void) override final
    {
        // Базовый метод
        base_t::refresh();
        
        // Следущий уровень яркости
        auto level_next = level_next_get();
        if (level_target != level_next)
        {
            level = math_value_ratio(level, level_target, frame, frame_count);
            level_target = level_next;
            frame = 0;
        }
        
        // Если фрейм конечный
        if (frame == frame_count)
            return;
        
        // Переход к следующему фрейму
        frame++;
        
        // Обработка разрядов
        for (hmi_rank_t i = 0; i < model_t::RANK_COUNT; i++)
        {
            auto data = base_t::in_get(i);
            data = data_get(data, level).smooth(data_get(data, level_target), frame, frame_count);
            
            base_t::out_set(i, data);
        }
    }
};

// Управление освещенности светодиодов
static class light_control_led_t : public light_control_t<led_model_t>
{
protected:
    // Получает данные относительно освещенности
    virtual data_t data_get(data_t source, uint8_t level) const override final
    {
        // Обработка отключения подсветки
        if (light_settings.autoset && light_settings.nightmode && level < 5)
            return data_t();
        
        const uint8_t DX = 10;
        return data_t().smooth(source, level + DX, LIGHT_LEVEL_MAX + DX);
    }
} light_control_led;

// Управление освещенности неонок
static class light_control_neon_t : public light_control_t<neon_model_t>
{
protected:
    // Получает данные относительно освещенности
    virtual data_t data_get(data_t source, uint8_t level) const override final
    {
        const uint8_t DX = 17;
        return data_t().smooth(source, level + DX, LIGHT_LEVEL_MAX + DX);
    }
} light_control_neon;

// Управление освещенности ламп
static class light_control_nixie_t : public light_control_t<nixie_model_t>
{
protected:
    // Получает данные относительно освещенности
    virtual data_t data_get(data_t source, uint8_t level) const override final
    {
        const uint8_t DX = 19;
        return data_t().smooth(source, level + DX, LIGHT_LEVEL_MAX + DX);
    }
} light_control_nixie;

// Сброс контроллеров
static void light_control_reset()
{
    light_control_led.reset();
    light_control_neon.reset();
    light_control_nixie.reset();
}

// Применение настроек в начале и при изменении
static void light_settings_apply(void)
{
    light_exposure_time_max = XK(light_settings.exposure);
    light_gain_coef_lux = powf(1.0615449167090511883376601260215f, light_settings.gain);
    light_level_flush();

    // Сброс контроллеров
    light_control_reset();
}

// Обработчик команды получения настроек освещенности
static class light_command_handler_settings_get_t : public ipc_responder_template_t<light_command_settings_get_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;

        // Подготовка данных
        command.response = light_settings;
        
        // Передача
        transmit();
    }
} light_command_handler_settings_get;

// Обработчик команды установки настроек освещенности
static class light_command_handler_settings_set_t : public ipc_responder_template_t<light_command_settings_set_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;

        // Применение настроек
        light_settings = command.request;
        light_settings_apply();
        storage_modified();
        
        // Передача
        transmit();
    }
} light_command_handler_settings_set;

// Обработчик команды получения состояния освещенности
static class light_command_handler_state_get_t : public ipc_responder_template_t<light_command_state_get_t>
{
protected:
    // Событие обработки данных
    virtual void work(bool idle) override final
    {
        if (idle)
            return;

        // Подготовка данных
        command.response.level = light_current_level;
        
        // Передача
        transmit();
    }
} light_command_handler_state_get;

void light_init(void)
{
    // Установка начального состояния
    light_measure_error();
    light_settings_apply();
    
    // Монтирование управления в экран
    screen.led.attach(light_control_led);
    screen.neon.attach(light_control_neon);
    screen.nixie.attach(light_control_nixie);
    
    // Обработчики IPC
    esp_handler_add(light_command_handler_state_get);
    esp_handler_add(light_command_handler_settings_get);
    esp_handler_add(light_command_handler_settings_set);
}

void light_setup_maximum(bool state)
{
    if (state)
    {
        if (light_setup_maximum_count++ > 0)
            return;
    }
    else
    {
        assert(light_setup_maximum_count > 0);
        if (--light_setup_maximum_count > 0)
            return;
    }
    
    light_control_reset();
}
