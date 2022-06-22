#include "mcu.h"
#include "nvic.h"
#include "temp.h"
#include "timer.h"
#include "xmath.h"
#include "system.h"

// Alias
#define DMA1_C4     DMA1_Channel4
#define DMA1_C5     DMA1_Channel5

// Аппаратный дополнительный делитель частоты UART
#define TEMP_UART_DIV_K                 16.0
// Вещественный делитель UART
#define TEMP_UART_DIV(baud)             (FMCU_NORMAL_HZ / ((float64_t)(baud)) / TEMP_UART_DIV_K)
// Целая часть делителя UART
#define TEMP_UART_DIV_INT(baud)         ((uint32_t)TEMP_UART_DIV(baud))
// Дробная часть делителя UART
#define TEMP_UART_DIV_FRAC(baud)        (TEMP_UART_DIV(baud) - TEMP_UART_DIV_INT(baud))
// Конвертирование дробной части делителя UART в целую
#define TEMP_UART_DIV_FRAC_I(baud)      ((uint32_t)(TEMP_UART_DIV_FRAC(baud) * TEMP_UART_DIV_K + 0.5))
// Подсчет регистра BRR по скорости
#define TEMP_UART_BRR_BY_BAUD(baud)     ((TEMP_UART_DIV_INT(baud) << 4) | (TEMP_UART_DIV_FRAC_I(baud) & 0x0F))

// Размер буфера DMA в байтах
#define TEMP_DMA_BUFFER_SIZE            32
// Получает, активно ли DMA
#define TEMP_DMA_IS_ACTIVE              (DMA1_C5->CCR & DMA_CCR_EN)

// Код определения присутствия ведомого
#define TEMP_DETECT_PRESENCE_CODE       0xF0

// Конверитрование бита в байт
#define TEMP_BIT_TO_BYTE(byte, bit)     ((byte) & (1 << (bit)) ? 0xFF : 0x00)
// Генерация байтового массива по байту
#define TEMP_BYTE_ARRAY_GEN(byte)       \
    TEMP_BIT_TO_BYTE(byte, 0),          \
    TEMP_BIT_TO_BYTE(byte, 1),          \
    TEMP_BIT_TO_BYTE(byte, 2),          \
    TEMP_BIT_TO_BYTE(byte, 3),          \
    TEMP_BIT_TO_BYTE(byte, 4),          \
    TEMP_BIT_TO_BYTE(byte, 5),          \
    TEMP_BIT_TO_BYTE(byte, 6),          \
    TEMP_BIT_TO_BYTE(byte, 7)

// Запрос к чипу для старта измерения
static const uint8_t TEMP_MEASURE_REQUEST_START[] =
{
    // Пропуск ROM (одно устройство на шине)
    TEMP_BYTE_ARRAY_GEN(0xCC),
    // Измерение
    TEMP_BYTE_ARRAY_GEN(0x44),
};

// Запрос к чипу для чтения результатов измерения
static const uint8_t TEMP_MEASURE_REQUEST_READ[] =
{
    // Пропуск ROM (одно устройство на шине)
    TEMP_BYTE_ARRAY_GEN(0xCC),
    // Чтение карты памяти
    TEMP_BYTE_ARRAY_GEN(0xBE),
    // Чтение двух байт
    TEMP_BYTE_ARRAY_GEN(0xFF),
    TEMP_BYTE_ARRAY_GEN(0xFF),
};

// Текущее состояние автомата
static __no_init enum
{
    // Простой
    TEMP_STATE_IDLE,
    // Старт измерения
    TEMP_STATE_MEASURE_START,
    // Ожидание измерения
    TEMP_STATE_MEASURE_WAIT,
    // Чтение результатов измерения
    TEMP_STATE_MEASURE_READING,
    // Завершение чтения результатов измерения
    TEMP_STATE_MEASURE_READING_FIN,
} temp_state;

// Буферы DMA
static __no_init struct
{
    uint8_t tx[TEMP_DMA_BUFFER_SIZE];
    uint8_t rx[TEMP_DMA_BUFFER_SIZE];
} temp_dma;

// Состояние сброса чипа
static __no_init bool temp_reseting;

// Текущее покзаание температуры
static __no_init float_t temp_current;

// Обработчик таймера таймаута операций ввода вывода (предварительное объявление)
static void temp_timeout_timer_cb(void);

// Таймер таймаута операций ввода вывода
static timer_callback_t temp_timeout_timer(temp_timeout_timer_cb);

// Перезапуск таймера таймаута операций ввода вывода
static void temp_timeout_timer_restart(void)
{
    // Всегда до 1 секунды
    temp_timeout_timer.start_hz(1);
}

// Переход в состояние простоя
static void temp_idle_state(uint32_t measure_timeout = XM(60))
{
    // Переход в начальное состояние
    temp_state = TEMP_STATE_IDLE;
    // Запуск таймера таумаута измерения
    temp_timeout_timer.start_us(measure_timeout);
}

// Инициализация канала DMA для датчика температуры
static void temp_dma_channel_init(DMA_Channel_TypeDef *channel, const void *mem, uint32_t flags)
{
    // Конфигурирование DMA
    mcu_dma_channel_setup_pm(channel,
        // Из UART
        USART1->DR, 
        // В память
        mem);
    channel->CCR |= DMA_CCR_MINC | flags;                                       // Direction (flags), Memory increment (8-bit), Peripheral size 8-bit
}

// Запуск DMA для датчика температуры
static void temp_dma_start(uint8_t size)
{
    // Проверка аргументов и состояний
    assert(size > 0);
    assert(!TEMP_DMA_IS_ACTIVE);
    // Запуск DMA
    DMA1_C4->CNDTR = DMA1_C5->CNDTR = size;                                     // Transfer data size
    DMA1_C5->CCR |= DMA_CCR_EN;                                                 // Channel enable
    DMA1_C4->CCR |= DMA_CCR_EN;                                                 // Channel enable
    // Запуск таймаута
    temp_timeout_timer_restart();
}

// Остановка работы DMA
static void temp_dma_stop(void)
{
    // Стоп таймера таймаута
    temp_timeout_timer.stop();
    // Стоп DMA
    DMA1->IFCR |= DMA_IFCR_CTCIF5;                                              // Clear CTCIF
    DMA1_C4->CCR &= ~DMA_CCR_EN;                                                // Channel disable
    DMA1_C5->CCR &= ~DMA_CCR_EN;                                                // Channel disable
}

// Сброс текущего значения
static void temp_current_set_reset(void)
{
    temp_current = 99.9f;
}

// Обработчик события ошибки чипа
static void temp_chip_error(void)
{
    // Аварийный стоп DMA
    temp_dma_stop();
    // Переход в начальное состояние
    temp_idle_state();
    // Сброс покзаания
    temp_current_set_reset();
}

// Запуск процедуры сброса чипа
static void temp_chip_reset(void)
{
    assert(!TEMP_DMA_IS_ACTIVE);
    // Состояние сброса
    temp_reseting = true;
    // Переход на низкую скорость
    USART1->BRR = TEMP_UART_BRR_BY_BAUD(9600);                                  // Baudrate 9600
    // Подготовка данных к передаче и отправка
    temp_dma.tx[0] = TEMP_DETECT_PRESENCE_CODE;
    temp_dma_start(1);
}

// Обработчик таймера таймаута операций ввода вывода
static void temp_timeout_timer_cb(void)
{
    switch (temp_state)
    {
        case TEMP_STATE_IDLE:
            // Подготовка следующего состояния
            temp_state = TEMP_STATE_MEASURE_START;
            // Сброс
            temp_chip_reset();
            break;
            
        case TEMP_STATE_MEASURE_READING:
            // Ожидание измерения, тут таймаут используется для паузы
            temp_chip_reset();
            break;
            
        default:
            // Истек таймаут операции DMA
            temp_chip_error();
            break;
    }
}

// Линеаризация текущей температуры
static void temp_current_linearization(void)
{
    // Точка для линеаризации (взято из графика в даташите)
    static const xmath_point2d_t<float_t, float_t> POINTS[] =
    {
        { 0.0f,     0.15f },
        { 10.0f,    0.18f },
        { 20.0f,    0.20f },
        { 30.0f,    0.18f },
        { 40.0f,    0.14f },
        { 50.0f,    0.07f },
        { 60.0f,    -0.01f },
        { 70.0f,    -0.14f },
    };
    
    temp_current += xmath_linear_interpolation(temp_current, POINTS, ARRAY_SIZE(POINTS));
}

// Обработчик события завершения передачи по DMA
static void temp_dma_event_cb(void)
{
    assert(temp_state != TEMP_STATE_IDLE);
    // Если сброс
    if (temp_reseting)
    {
        temp_reseting = false;
        // Если передали то же что и приняли - никого нет
        if (temp_dma.rx[0] == TEMP_DETECT_PRESENCE_CODE)
        {
            temp_chip_error();
            return;
        }
        // Переход на высокую скорость
        USART1->BRR = TEMP_UART_BRR_BY_BAUD(115200);                            // Baudrate 115200
    }
    
    // Далее по состояниям
    switch (temp_state)
    {
        case TEMP_STATE_IDLE:
            // Пропуск
            break;
            
        case TEMP_STATE_MEASURE_START:
            // Запрос
            memcpy(temp_dma.tx, TEMP_MEASURE_REQUEST_START, sizeof(TEMP_MEASURE_REQUEST_START));
            temp_dma_start(sizeof(TEMP_MEASURE_REQUEST_START));
            // Далее ожидание результатов измерения
            temp_state = TEMP_STATE_MEASURE_WAIT;
            break;
            
        case TEMP_STATE_MEASURE_WAIT:
            // Запуск таймаута
            temp_timeout_timer_restart();
            // Далее чтение результатов
            temp_state = TEMP_STATE_MEASURE_READING;
            break;
            
        case TEMP_STATE_MEASURE_READING:
            // Запрос
            {
                memcpy(temp_dma.tx, TEMP_MEASURE_REQUEST_READ, sizeof(TEMP_MEASURE_REQUEST_READ));
                temp_dma_start(sizeof(TEMP_MEASURE_REQUEST_READ));
            }
            // Далее завершение чтения результатов
            temp_state = TEMP_STATE_MEASURE_READING_FIN;
            break;
            
        case TEMP_STATE_MEASURE_READING_FIN:
            temp_current = 0;
            for (auto i = 16; i < 27; i++)
            {
                // Только установленные биты
                if (temp_dma.rx[i] != 0xFF)
                    continue;
                // Степень
                auto base = i - 16 - 4;
                auto neg = base < 0;
                if (neg)
                    base = -base;
                // Возвещение в степень
                auto pow = 1 << base;
                // Инкремент разряда
                temp_current += neg ? 1 / pow : pow;
            }
            // Учет знака
            if (temp_dma.rx[28] == 0xFF)
                temp_current = -temp_current;
            // Линеаризация
            temp_current_linearization();
            // В начальное состояние
            temp_idle_state();
            break;
    }
}

// Событие завершения передачи по DMA
static event_callback_t temp_dma_event(temp_dma_event_cb);

void temp_init(void)
{
    // Тактирование и сброс USART1
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;                                       // USART1 clock enable
    RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST;                                    // USART1 reset
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;                                   // USART1 unreset
    
    // Конфигурирование UART
    USART1->CR1 = USART_CR1_UE;                                                 // UART enable, Parity off, 8-bit, Wakeup idle line, IRQ off, RX off, TX off
    USART1->CR2 = 0;                                                            // 1 stop bit, Clock off
    USART1->CR3 = USART_CR3_HDSEL | USART_CR3_DMAR | USART_CR3_DMAT;            // Half-Duplex enable, CTS off, RTS off, DMA TX RX enable
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;                                 // TX RX enable
    
    // Конфигурирование DMA
    DMA1->IFCR |= DMA_IFCR_CTCIF5;                                              // Clear CTCIF
    // Включаем прерывание канала DMA
    nvic_irq_enable_set(DMA1_Channel5_IRQn, true);
    // Канал 4 (TX)
    temp_dma_channel_init(DMA1_C4, temp_dma.tx, DMA_CCR_DIR);                   // Memory to peripheral
    // Канал 5 (RX)
    temp_dma_channel_init(DMA1_C5, temp_dma.rx, DMA_CCR_TCIE);                  // Transfer complete IRQ enable
    
    // Начальное состояние
    temp_current_set_reset();
    // Форсирование первого измерения
    temp_idle_state(TIMER_US_MIN);
}

float_t temp_current_get(void)
{
    return temp_current;
}

IRQ_ROUTINE
void temp_interrupt_dma(void)
{
    // DMA
    temp_dma_stop();
    // Событие
    temp_dma_event.raise();
}
