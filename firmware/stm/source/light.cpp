#include "esp.h"
#include "mcu.h"
#include "light.h"
#include "timer.h"
#include "xmath.h"
#include "system.h"
#include "screen.h"
#include "random.h"

// Максимальное значение уровня освещенности
constexpr const uint8_t LIGHT_LEVEL_MAX = 100;

// Количество измерений в секунду (не менять)
constexpr const uint8_t LIGHT_MSR_PER_SECOND = 5;
// Количество секунд обновления уровня освещенности
constexpr const uint16_t LIGHT_LEVEL_UPDATE_SECONDS = 5;

// Генерация начала транзакции
static bool light_wire_start(bool is_read)
{
    // Софтовый сброс I2C
    I2C1->CR1 |= I2C_CR1_SWRST;                                                 // SW reset
    I2C1->CR1 &= ~I2C_CR1_SWRST;                                                // SW unreset

    // Конфигурирование I2C1
    I2C1->CR2 = I2C_CR2_FREQ_4 | I2C_CR2_FREQ_5;                                // APB 48 MHz
    I2C1->CCR = (I2C_CCR_CCR & 5) | I2C_CCR_DUTY | I2C_CCR_FS;                  // 400 KHz @ APB 48 MHz, FM, 16:9
    I2C1->CR1 = I2C_CR1_PE;                                                     // I2C on
    
    // Фаза старта
    I2C1->CR1 |= I2C_CR1_START | I2C_CR1_ACK;                                   // Start, ACK
    if (is_read)
        I2C1->CR1 |= I2C_CR1_POS;                                               // NACK on 2nd byte
    if (!mcu_pool_ms([](void) -> bool
        {
            return (I2C1->SR1 & I2C_SR1_SB) != 0;                               // Check SB
        }))
        return false;
    
    // Фаза адреса
    I2C1->DR = 0x46 | is_read;                                                  // Send address
    if (!mcu_pool_ms([](void) -> bool
        {
            return I2C1->SR1 != 0;                                              // Check SR1
        }))
        return false;

    return ((I2C1->SR1 & I2C_SR1_AF) == 0) &&                                   // Check ACK
           ((I2C1->SR2 & I2C_SR2_MSL) != 0);                                    // Check master mode
}

// Генерация завершения транзакции
static bool light_wire_stop(void)
{
    // Ожидание передачи данных
    if (!mcu_pool_ms([](void) -> bool
        {
            return (I2C1->SR1 & I2C_SR1_BTF) != 0;                              // Check BTF
        }))
        return false;
    
    // Фаза стопа
    I2C1->CR1 |= I2C_CR1_STOP;
    return mcu_pool_ms([](void) -> bool
        {
            return (I2C1->SR2 & I2C_SR2_BUSY) == 0;                             // Check BUSY
        });
}

// Проверка завыершения транзакции
static bool light_wire_finalize(void)
{
    return (I2C1->SR2 == 0) && (I2C1->SR1 == 0);                                // Check SR1/2
}

// Чтение данных по I2C
static bool light_wire_read(uint8_t &dr0, uint8_t &dr1)
{
    if (!light_wire_start(true))
        return false;
    
    // Сброс подтверждения
    I2C1->CR1 &= ~I2C_CR1_ACK;                                                  // Clear ACK
    
    if (!light_wire_stop())
        return false;

    dr0 = I2C1->DR;                                                             // Read first byte
    dr1 = I2C1->DR;                                                             // Read second byte

    return light_wire_finalize();
}

// Запись байта данных по I2C
static bool light_wire_write(uint8_t opcode)
{
    if (!light_wire_start(false))
        return false;
    
    // Ожидание байта
    if (!mcu_pool_ms([](void) -> bool
        {
            return (I2C1->SR1 & I2C_SR1_TXE) != 0;                              // Check TXE
        }))
        return false;
    
    I2C1->DR = opcode;                                                          // Write TX byte
    
    // Завершение
    return light_wire_stop() && 
           light_wire_finalize();
}

// Обработчик таймера автомата состояний (предварительное объявление)
static void light_state_timer_cb(void);

// Текущее состояние автомата
static __no_init enum light_state_t
{
    // Конфигурирование
    LIGHT_STATE_CONGIF,
    // Чтение результатов
    LIGHT_STATE_READING,
} light_state;

// Максимальное покзаание в люксах
static __no_init float_t light_max_lux;
// Текущее покзаание в люксах
static __no_init float_t light_current_lux;
// Текущее показание уровня освещенности
static __no_init uint8_t light_current_level;
// Прескалер обновления уровня освещенности
static __no_init uint16_t light_level_update_prescaler;

// Таймер автомата состояний
static timer_t light_state_timer(light_state_timer_cb);

// Обновление уровня освещенности
static void light_level_update(void)
{
    // Откладываем обновление
    light_level_update_prescaler = 0;
    
    // Точки линеаризации
    static const math_point2d_t<float_t, uint8_t> POINTS[] =
    {
        { 0.0f,     0 },
        { 5.0f,     20 },
        { 10.0f,    40 },
        { 20.0f,    60 },
        { 40.0f,    80 },
        { 70.0f,    100 },
    };
    
    // Интерполяция
    light_current_lux = light_max_lux;
    light_current_level = math_linear_interpolation(light_current_lux, POINTS, array_length(POINTS));
    light_max_lux = 0.0f;
}

// Повтор выполнения текущего состояния
static void light_state_retry(void)
{
    light_state_timer.start_us(TIMER_US_MIN);
}

// Установка таймера для задержки состояния
static void light_state_delay(void)
{
    light_state_timer.start_hz(LIGHT_MSR_PER_SECOND);
}

// Обрабогтчик ошибки измерения
static void light_measure_error(void)
{
    // Текущее значение не известно
    light_current_lux = NAN;
    // Переход к конфигурированию
    light_state = LIGHT_STATE_CONGIF;
    // Задержка обработки
    light_state_delay();
}

// Обработчик таймера автомата состояний
static void light_state_timer_cb(void)
{
    /* Если активна линия ESP, то откладываем
     * В Errata запрещено использование SPI1 и I2C в случае ремапа */
    if (esp_wire_active())
    {
        light_state_retry();
        return;
    }
    
    // Включение I2C1
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;                                         // I2C1 clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;                                      // I2C1 reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;                                     // I2C1 unreset
    
    switch (light_state)
    {
        case LIGHT_STATE_CONGIF:
            // Питание, тайминг, измерение
            if (light_wire_write(0x01) && 
                light_wire_write(0x47) &&
                light_wire_write(0x47) &&
                light_wire_write(0x7E) &&
                light_wire_write(0x11))
                // Ожидание 200 мС
            {
                // Переход к чтению
                light_state = LIGHT_STATE_READING;
                // Задержка обработки
                light_state_delay();
                break;
            }
            
            // Опа...
            light_measure_error();
            break;
            
        case LIGHT_STATE_READING:
            {
                // Чтение результатов
                union
                {
                    uint16_t raw;
                    struct
                    {
                        uint8_t lsb, msb;
                    };
                };
                
                if (!light_wire_read(msb, lsb))
                {
                    light_measure_error();
                    break;
                }

                // В рандом младший бит
                random_noise_bit((raw & 2) != 0);
                
                // Конвертирование
                float_t lux;
                {
                    // Точность
                    constexpr const auto ACCURACY = 1.2f;
                    // Значение регистра тайминга по умолчанию
                    constexpr const auto MTREG_DEF = 69.0f;
                    // Текущее значение регистра тайминга
                    constexpr const auto MTREG_CUR = 254.0f;
                    // Делитель при высокой точности
                    constexpr const auto HIRES_DIV = 2.0f;
                    // Коофициент трансформации
                    constexpr const auto COEFF = 1.0f / ACCURACY * (MTREG_DEF / MTREG_CUR) / HIRES_DIV;

                    // Пересчет
                    lux = raw * COEFF;
                }
                
                // Задержка обработки
                light_state_delay();
                
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
                
                // Прескалер обновления
                if (++light_level_update_prescaler >= 
                    LIGHT_MSR_PER_SECOND * LIGHT_LEVEL_UPDATE_SECONDS)
                    light_level_update();
            }
            break;
    }
    
    // Отключение I2C1
    RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;                                        // I2C1 clock disable
}


// Класс захвата источника данных
template <typename MODEL, hmi_sat_t DX>
class light_data_control_t : public MODEL::transceiver_t
{
    // Псевдонимы
    using model_t = MODEL;
    using data_t = typename model_t::data_t;
    using base_t = typename model_t::transceiver_t;

    // Текущее значение яркости
    uint8_t level = LIGHT_LEVEL_MAX;
    
    // Получает финальные данные относительно освещенности
    data_t final_data_get(data_t source) const 
    {
        return data_t().smooth(source, level + DX, LIGHT_LEVEL_MAX + DX);
    }
    
public:
    // Контроллер плавного изменения
    typename model_t::smoother_t smoother;
    
    // Конструктор по умолчанию
    light_data_control_t(void)
    {
        // TODO: применять из настроек
        smoother.time_set(2);
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
        
    // Обработчик изменения стороны
    virtual void side_changed(list_side_t side) override final
    {
        // Базовый метод
        base_t::side_changed(side);
        
        // Передача напрямую если подключились перед нами
        if (side == LIST_SIDE_PREV)
            smoother.stop();
    }
    
    // Обработчик изменения данных
    virtual void data_changed(hmi_rank_t index, data_t &data) override final
    {
        // Базовый метод
        base_t::out_set(index, final_data_get(data));
    }
    
    // Обнвление данных
    virtual void refresh(void) override final
    {
        // Базовый метод
        base_t::refresh();
        
        // Если уровень освещения изменился
        const bool start_effect = level != light_current_level;
        if (start_effect)
            level = light_current_level;
        
        // Обработка разрядов
        for (hmi_rank_t i = 0; i < model_t::RANK_COUNT; i++)
        {
            // Если уровень освещения изменился
            if (start_effect)
                smoother.start(i, base_t::out_get(i));

            // Конечные данные
            const auto to = final_data_get(base_t::in_get(i));
            
            // Если эффект перехода активен
            if (smoother.process_needed(i))
                base_t::out_set(i, smoother.process(i, to));
            else
                base_t::out_set(i, to);
        }
    }
};

// Управление освещенности светодиодов
static light_data_control_t<led_model_t, 10> light_led_control;
// Управление освещенности неонок
static light_data_control_t<neon_model_t, 17> light_neon_control;
// Управление освещенности ламп
static light_data_control_t<nixie_model_t, 19> light_nixie_control;

void light_init(void)
{
    // Изначально счтиаем что максимально
    light_current_level = LIGHT_LEVEL_MAX;
    // Изначально в состояние ошибки
    light_measure_error();
    // Форсирование первого измерения
    light_state_retry();
    
    // Монтирование управления в экран
    screen.led.attach(light_led_control);
    screen.neon.attach(light_neon_control);
    screen.nixie.attach(light_nixie_control);
}
