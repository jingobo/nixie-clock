#ifndef __I2C_H
#define __I2C_H

#include "io.h"
#include "mcu.h"

// Производит инициалзиацию периферии
inline void i2c_init(void)
{
    // Включение I2C1
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;                                         // I2C1 clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;                                      // I2C1 reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;                                     // I2C1 unreset
    
    // Софтовый сброс I2C
    I2C1->CR1 |= I2C_CR1_SWRST;                                                 // SW reset
    I2C1->CR1 &= ~I2C_CR1_SWRST;                                                // SW unreset

    // Конфигурирование I2C1
    I2C1->CR2 = I2C_CR2_FREQ_4 | I2C_CR2_FREQ_5;                                // APB 48 MHz
    I2C1->CCR = (I2C_CCR_CCR & 5) | I2C_CCR_DUTY | I2C_CCR_FS;                  // 400 KHz @ APB 48 MHz, FM, 16:9
    I2C1->CR1 = I2C_CR1_PE;                                                     // I2C on
}

// Производит деинициалзиацию периферии
inline void i2c_deinit(void)
{
    // Отключение I2C1
    RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;                                        // I2C1 clock disable
}

// Устанавливает SCL как PP low
inline void i2c_scl_low(void)
{
    IO_LS_SCL_CR &= ~IO_LS_SCL_ALT;
}

// Устанавливает SCL как Alt PP
inline void i2c_scl_alt(void)
{
    IO_LS_SCL_CR |= IO_LS_SCL_ALT;
}

// Перечисление режимов I2C
enum i2c_mode_t
{
    // Запись
    I2C_MODE_WRITE,
    // Чтение
    I2C_MODE_READ,
    // Чтение двух байт
    I2C_MODE_READ_X2,
};

// Производит запуск транзакции
inline bool i2c_start(uint8_t address, i2c_mode_t mode)
{
    // Фаза старта
    I2C1->CR1 |= I2C_CR1_START | I2C_CR1_ACK;                                   // Start, ACK
    if (mode == I2C_MODE_READ_X2)
        I2C1->CR1 |= I2C_CR1_POS;                                               // NACK on 2nd byte
    
    // Фаза адреса
    if (!mcu_pool_ms([](void) -> bool
        {
            return (I2C1->SR1 & I2C_SR1_SB) != 0;                               // Check SB
        }))
        return false;
    
    I2C1->DR = address << 1 | mode != I2C_MODE_WRITE;                           // Send address
    
    // Ожидание ADDR
    if (!mcu_pool_ms([](void) -> bool
        {
            return I2C1->SR1 != 0;                                              // Check SR1
        }))
        return false;

    // Errata
    if (mode == I2C_MODE_READ_X2)
        i2c_scl_low();

    // Сброс ADDR
    return ((I2C1->SR1 & I2C_SR1_AF) == 0) &&                                   // Check ACK
           ((I2C1->SR2 & I2C_SR2_MSL) != 0);                                    // Check master mode
}

// Производит запуск последовательности стопа
inline void i2c_stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;                                                  // Stop
}

// Производит установку отсутствия подтверждения
inline void i2c_nack(void)
{
    I2C1->CR1 &= ~I2C_CR1_ACK;                                                  // NACK
}

// Ожидает освобождение буфера к передаче
inline bool i2c_wait_txe(void)
{
    return mcu_pool_ms([](void) -> bool
    {
        return (I2C1->SR1 & I2C_SR1_TXE) != 0;                                  // Check TXE
    });
}

// Ожидает завершение передачи
inline bool i2c_wait_btf(void)
{
    return mcu_pool_ms([](void) -> bool
    {
        return (I2C1->SR1 & I2C_SR1_BTF) != 0;                                  // Check BTF
    });
}

// Ожидает данные к чтению
inline bool i2c_wait_rxne(void)
{
    return mcu_pool_ms([](void) -> bool
    {
        return (I2C1->SR1 & I2C_SR1_RXNE) != 0;                                 // Check RXNE
    });
}

// Проверка завыершения транзакции
inline bool i2c_finalize(void)
{
    if (!mcu_pool_ms([](void) -> bool
        {
            return (I2C1->SR2 & I2C_SR2_BUSY) == 0;                             // Check BUSY
        }))
        return false;
    
    return (I2C1->SR2 == 0) && (I2C1->SR1 == 0);                                // Check SR1/2
}

// Производит чтение байта
inline uint8_t i2c_read(void)
{
    return I2C1->DR;                                                            // Read RX byte
}

// Производит запись байта
inline void i2c_write(uint8_t data)
{
    I2C1->DR = data;                                                            // Write TX byte
}

// Производит чтение двух байт
inline bool i2c_read_x2(uint8_t address, uint8_t &dr1, uint8_t &dr2)
{
    // Открытие на чтение
    if (!i2c_start(address, I2C_MODE_READ_X2))
    {
        // Errata
        i2c_scl_alt();
        return false;
    }

    // NAK предварительно
    i2c_nack();
    i2c_scl_alt();

    // Приём двух байт
    if (!i2c_wait_btf())
        return false;

    // Errata
    i2c_scl_low();
    
    // Перед чтением стоп
    i2c_stop();
    
    // Первый байт
    dr1 = i2c_read();
    
    // Errata
    i2c_scl_alt();
    
    // Второй байт
    dr2 = i2c_read();
    
    // Завершение стопа
    return i2c_finalize();
}

#endif // __I2C_H
