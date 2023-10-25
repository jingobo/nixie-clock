#include "mcu.h"
#include "nvic.h"
#include "temp.h"
#include "timer.h"
#include "xmath.h"
#include "system.h"

// Используемый канал DMA TX
static DMA_Channel_TypeDef * const TEMP_DMA_C4 = DMA1_Channel4;
// Используемый канал DMA RX
static DMA_Channel_TypeDef * const TEMP_DMA_C5 = DMA1_Channel5;

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
    // Прототип буфера DMA
    typedef uint8_t buffer_t[88];
    
    // Передача
    buffer_t tx;
    // Приём
    buffer_t rx;
} temp_dma;

// Состояние сброса чипа
static __no_init bool temp_reseting;

// Текущее покзаание температуры
static __no_init float_t temp_current;

// Обработчик таймера таймаута операций ввода вывода (предварительное объявление)
static void temp_timeout_timer_cb(void);

// Таймер таймаута операций ввода вывода
static timer_t temp_timeout_timer(temp_timeout_timer_cb);

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

// Получает, не активно ли DMA (для assert)
#define TEMP_DMA_IS_INACTIVE      ((TEMP_DMA_C5->CCR & DMA_CCR_EN) == 0)

// Запуск DMA для датчика температуры
static void temp_dma_start(uint8_t size)
{
    // Проверка аргументов и состояний
    assert(size > 0);
    assert(size <= sizeof(temp_dma.tx));
    assert(TEMP_DMA_IS_INACTIVE);
    
    // Запуск DMA
    TEMP_DMA_C4->CNDTR = TEMP_DMA_C5->CNDTR = size;                             // Transfer data size
    TEMP_DMA_C5->CCR |= DMA_CCR_EN;                                             // Channel enable
    TEMP_DMA_C4->CCR |= DMA_CCR_EN;                                             // Channel enable
    
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
    TEMP_DMA_C4->CCR &= ~DMA_CCR_EN;                                            // Channel disable
    TEMP_DMA_C5->CCR &= ~DMA_CCR_EN;                                            // Channel disable
}

// Сброс текущего значения
static void temp_current_reset(void)
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
    temp_current_reset();
}

// Подсчет регистра BRR по скорости
constexpr uint32_t temp_uart_brr_by_baud(uint32_t baud)
{
    // Аппаратный дополнительный делитель частоты UART
    const auto DIV_K = 16.0;
    
    // Вещественный делитель UART
    const auto DIV = FMCU_NORMAL_HZ / ((float64_t)baud) / DIV_K;
    // Целая часть делителя UART
    const auto DIV_I = (uint32_t)DIV;
    
    // Дробная часть делителя UART
    const auto DIV_FRAC = DIV - DIV_I;
    // Конвертирование дробной части делителя UART в целую
    const auto DIV_FRAC_I = (uint32_t)(DIV_FRAC * DIV_K + 0.5);

    // Конечный подсчет регистра BRR
    return (DIV_I << 4) | (DIV_FRAC_I & 0x0F);
}

// Код определения присутствия ведомого
constexpr const uint8_t TEMP_DETECT_PRESENCE_CODE = 0xF0;

// Запуск процедуры сброса чипа
static void temp_chip_reset(void)
{
    assert(TEMP_DMA_IS_INACTIVE);
    
    // Состояние сброса
    temp_reseting = true;
    // Переход на низкую скорость
    USART1->BRR = temp_uart_brr_by_baud(9600);                                  // Baudrate 9600
    
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

// Конверитрование бита в байт
constexpr uint8_t temp_bit_to_byte(uint8_t byte, uint8_t bit)
{
    return (byte & (1 << bit)) ? 0xFF : 0x00;
}

// Расчет контрольной суммы дныых датчика
uint8_t temp_dallas_crc(const void *data)
{
	uint8_t crc = 0;
    const uint8_t *u8 = (const uint8_t *)data;
	
	for (auto i = 0; i < 8; i++)
    {
		auto temp = *u8++;
		for (auto j = 0; j < 8; j++, temp >>= 1)
        {
			const auto mix = (crc ^ temp) & 0x01;
			crc >>= 1;
			
            if (mix) 
                crc ^= 0x8C;
		}
	}
    
	return crc;
}

// Генерация байтового массива по байту
#define TEMP_BYTE_ARRAY_GEN(byte)       \
    temp_bit_to_byte(byte, 0),          \
    temp_bit_to_byte(byte, 1),          \
    temp_bit_to_byte(byte, 2),          \
    temp_bit_to_byte(byte, 3),          \
    temp_bit_to_byte(byte, 4),          \
    temp_bit_to_byte(byte, 5),          \
    temp_bit_to_byte(byte, 6),          \
    temp_bit_to_byte(byte, 7)

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
        USART1->BRR = temp_uart_brr_by_baud(115200);                            // Baudrate 115200
    }
    
    // Далее по состояниям
    switch (temp_state)
    {
        case TEMP_STATE_IDLE:
            // Пропуск
            break;
            
        case TEMP_STATE_MEASURE_START:
            // Запрос
            {
                // Запрос к чипу для старта измерения
                static const uint8_t REQUEST[] =
                {
                    // Пропуск ROM (одно устройство на шине)
                    TEMP_BYTE_ARRAY_GEN(0xCC),
                    // Измерение
                    TEMP_BYTE_ARRAY_GEN(0x44),
                };
                
                memcpy(temp_dma.tx, REQUEST, sizeof(REQUEST));
                temp_dma_start(sizeof(REQUEST));
            }
            
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
                // Запрос к чипу для чтения результатов измерения
                static const uint8_t REQUEST[] =
                {
                    // Пропуск ROM (одно устройство на шине)
                    TEMP_BYTE_ARRAY_GEN(0xCC),
                    // Чтение карты памяти
                    TEMP_BYTE_ARRAY_GEN(0xBE),
                    // Чтение 9 байт
                    TEMP_BYTE_ARRAY_GEN(0xFF), TEMP_BYTE_ARRAY_GEN(0xFF),
                    TEMP_BYTE_ARRAY_GEN(0xFF), TEMP_BYTE_ARRAY_GEN(0xFF),
                    TEMP_BYTE_ARRAY_GEN(0xFF), TEMP_BYTE_ARRAY_GEN(0xFF),
                    TEMP_BYTE_ARRAY_GEN(0xFF), TEMP_BYTE_ARRAY_GEN(0xFF),
                    TEMP_BYTE_ARRAY_GEN(0xFF),
                };
                
                memcpy(temp_dma.tx, REQUEST, sizeof(REQUEST));
                temp_dma_start(sizeof(REQUEST));
            }
            
            // Далее завершение чтения результатов
            temp_state = TEMP_STATE_MEASURE_READING_FIN;
            break;
            
        case TEMP_STATE_MEASURE_READING_FIN:
            {
                // Индексы полей структуры данных датчика
                enum
                {
                    // Младшая часть значения
                    POS_LSB,
                    // Старшая часть значения
                    POS_MSB,
                    // Верхний аварийный порог
                    POS_TH,
                    // Нижний аварийный порог
                    POS_TL,
                    // Конфигурация
                    POS_CFG,
                    // Первое магическое значение
                    POS_MAGIC_1,
                    // Резерв
                    POS_RESERVED,
                    // Второе магическое значение
                    POS_MAGIC_2,
                    // Контрольная сумма
                    POS_CRC
                };
                
                union
                {
                    // Сырое значение
                    uint8_t u8;
                    // Старшая часть значения
                    struct
                    {
                        uint8_t value : 3;
                        bool sign : 1;
                    } msb;
                } raw[9] = { 0 };
                
                // Сброка по битам
                for (auto i = 0; i < sizeof(raw); i++)
                {
                    const auto base = i * 8 + 16;
                    for (auto j = base; j < base + 8; j++)
                    {
                        raw[i].u8 >>= 1;
                        if (temp_dma.rx[j] == 0xFF)
                            raw[i].u8 |= 0x80;
                    }
                    
                }
                
                // Проверка контрольной суммы
                if (raw[POS_MAGIC_1].u8 != 0xFF || 
                    raw[POS_MAGIC_2].u8 != 0x10 || 
                    temp_dallas_crc(raw) != raw[POS_CRC].u8)
                {
                    // Ошибка и в начальное состояние
                    temp_current_reset();
                    temp_idle_state();
                    break;
                }
                
                // Целое значение температуры
                uint16_t u16 = raw[POS_MSB].msb.value;
                u16 <<= 8;
                u16 |= raw[POS_LSB].u8;

                // Вещественное значение температуры
                temp_current = u16;
                temp_current /= 16.0;

                // Учет знака
                if (raw[POS_MSB].msb.sign)
                    temp_current = -temp_current;
            }
            
            // Линеаризация
            {
                // Точка для линеаризации (взято из графика в даташите)
                static const math_point2d_t<float_t, float_t> POINTS[] =
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
                
                temp_current += math_linear_interpolation(temp_current, POINTS, array_length(POINTS));
            }
            
            // В начальное состояние
            temp_idle_state();
            break;
    }
}

// Событие завершения передачи по DMA
static event_t temp_dma_event(temp_dma_event_cb);

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
    nvic_irq_enable(DMA1_Channel5_IRQn);
    // Канал 4 (TX)
    temp_dma_channel_init(TEMP_DMA_C4, temp_dma.tx, DMA_CCR_DIR);               // Memory to peripheral
    // Канал 5 (RX)
    temp_dma_channel_init(TEMP_DMA_C5, temp_dma.rx, DMA_CCR_TCIE);              // Transfer complete IRQ enable
    
    // Начальное состояние
    temp_current_reset();
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
