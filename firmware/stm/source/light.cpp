#include "esp.h"
#include "mcu.h"
#include "light.h"
#include "timer.h"
#include "system.h"

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
            return I2C1->SR1 & I2C_SR1_SB != 0;                                 // Check SB
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
static bool light_wire_read(uint8_t &msb, uint8_t &lsb)
{
    if (!light_wire_start(true))
        return false;
    
    // Сброс подтверждения
    I2C1->CR1 &= ~I2C_CR1_ACK;                                                  // Clear ACK
    
    if (!light_wire_stop())
        return false;

    msb = I2C1->DR;                                                             // Read MSB
    lsb = I2C1->DR;                                                             // Read LSB

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
    // Старт измерения
    LIGHT_STATE_MEASURE,
    // Завершение измерения
    LIGHT_STATE_MEASURE_FIN,
} light_state;

// Текущее покзаание в люксах
static __no_init sensor_value_t light_current;

// Таймер автомата состояний
static timer_callback_t light_state_timer(light_state_timer_cb);

// Установка состояния
static void light_state_set(light_state_t state, uint32_t us)
{
    light_state = state;
    light_state_timer.start_us(us);
}

// Повтор выполнения текущего состояния
static void light_state_retry(void)
{
    light_state_timer.start_us(TIMER_US_MIN);
}

// Обработчик завершения измерения
static void light_measure_done(void)
{
    // Повторное измерение через 1 секунду
    light_state_set(LIGHT_STATE_MEASURE, XM(1));
}

// Обрабогтчик ошибки измерения
static void light_measure_error(void)
{
    // Сброс показания
    light_current = SENSOR_VALUE_EMPTY;
    // Завершение измерения
    light_measure_done();
}

// Обработчик таймера автомата состояний
static void light_state_timer_cb(void)
{
    /* Если активна линия ESP, то откладываем
     * В Errata запрещено использование SPI1 и I2C в случае ремапа */
    if (esp_wire_active())
        light_state_retry();
    
    // Включение I2C1
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;                                         // I2C1 clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;                                      // I2C1 reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;                                     // I2C1 unreset
    
    switch (light_state)
    {
        case LIGHT_STATE_MEASURE:
            // Питание, измерение
            if (light_wire_write(0x01) && light_wire_write(0x20))
                // Ожидание 200 мС
                light_state_set(LIGHT_STATE_MEASURE_FIN, XK(200));
            else
                // Опа...
                light_measure_error();
            break;
        case LIGHT_STATE_MEASURE_FIN:
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
                
                // Конвертирование
                auto current = raw * 0.83f;
                
                // Гестерезис в 5
                if (abs(light_current - current) > 5)
                    light_current = current;
                light_measure_done();
            }
            break;
    }
    
    // Отключение I2C1
    RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;                                        // I2C1 clock disable
}

void light_init(void)
{
    // Изначально в состояние ошибки
    light_measure_error();
    // Форсирование первого измерения
    light_state_retry();
}

sensor_value_t light_current_get(void)
{
    return light_current;
}
