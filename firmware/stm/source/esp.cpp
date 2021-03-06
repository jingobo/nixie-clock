﻿#include "io.h"
#include "esp.h"
#include "mcu.h"
#include "nvic.h"
#include "event.h"
#include "timer.h"

// Alias
#define DMA1_C2                 DMA1_Channel2
#define DMA1_C3                 DMA1_Channel3

// Команда ESP8266 TX & RX подчиненного устройства
#define ESP_SPI_CMD_RD_WR       0x06
// Частота передачи пакетов IPC по SPI
#define ESP_SPI_IPC_TX_HZ       75
// Время ожидания после смены состояния выводов ESP8266
#define ESP_PIN_SWUTCH_US       XK(50)
// Время ожидания инициализации чипа ESP8266
#define ESP_START_TIME_US       XK(500)

// Предварительное объявление
static void esp_reset_do(void);

// Ввод/вывод
static struct esp_io_t
{
    // Буфер пакета для DMA
    struct
    {
        // Для выравнивания к четному адресу
        uint8_t pads[3];
        // Байт команды для ESP8266
        uint8_t command;
        // Пакет приёма и передачи
        ipc_packet_t tx, rx;
    } dma;
    // Флаг активности транзакцими
    bool active;

    // Конструктор по умолчанию
    esp_io_t(void) : active(false)
    { }
} esp_io;

// Хост обработчиков команд
static ipc_handler_host_t esp_handler_host;

// Класс связи с ESP
static class esp_link_t : public ipc_link_master_t
{
protected:
    // Массовый сброс (другая сторона не отвечает)
    virtual void reset_slave(void) override final
    {
        // Сброс чипаа
        esp_reset_do();
    }
public:
    // Ввод полученного пакета
    virtual bool packet_input(const ipc_packet_t &packet) override final
    {
        // Базовый метод
        auto result = ipc_link_master_t::packet_input(packet);
        // Обработка
        if (result && !packet.dll.more)
            flush_packets(esp_handler_host);
        return true;
    }
} esp_link;

// Инициализация канала DMA
static void esp_dma_channel_init(DMA_Channel_TypeDef *channel, uint32_t flags)
{
    // Конфигурирование DMA
    mcu_dma_channel_setup_pm(channel,
        // Из SPI
        SPI1->DR, 
        // В память
        &esp_io.dma.command);
    channel->CCR |= DMA_CCR_MINC | DMA_CCR_PL_1 | flags;                        // Direction (flags), Memory increment (8-bit), Peripheral size 8-bit, High priority
}

// Таймер начала ввода/вывода
static timer_callback_t esp_io_begin_timer([](void)
{
    if (esp_io.active)
        return;
    esp_io.active = true;
    // Опрос команд
    esp_handler_host.pool();
    // Подготовка данных
    esp_link.packet_output(esp_io.dma.tx);
    esp_io.dma.command = ESP_SPI_CMD_RD_WR;
    // Начало передачи
    IO_PORT_RESET(IO_ESP_CS);                                                   // Slave select
    // DMA
    DMA1_C2->CNDTR = DMA1_C3->CNDTR = sizeof(esp_io.dma);                       // Transfer data size
    DMA1_C2->CCR |= DMA_CCR_EN;                                                 // Channel enable
    DMA1_C3->CCR |= DMA_CCR_EN;                                                 // Channel enable
});

// Событие завершения ввода/вывода
static event_callback_t esp_io_complete_event([](void)
{
    esp_io.active = false;
    // Извлекаем пакет
    esp_link.packet_input(esp_io.dma.rx);
});

// Состояние сброса
static enum
{
    // Сброс не происходит
    ESP_RESET_STATE_IDLE = 0,
    // Сброс (RST на землю)
    ESP_RESET_STATE_RESET,
    // Загрузка (BOOT0 к питанию)
    ESP_RESET_STATE_BOOT,
    // Ожидание инициализации
    ESP_RESET_STATE_INIT
} esp_reset_state = ESP_RESET_STATE_IDLE;

// Таймер обработки текущего состояния сброса
static timer_callback_t esp_reset_timer([](void)
{
    switch (esp_reset_state)
    {
        case ESP_RESET_STATE_RESET:
            IO_PORT_SET(IO_ESP_RST);                                            // Esp unreset
            // Таймер на бутлоадер
            esp_reset_timer.start_us(ESP_PIN_SWUTCH_US);
            // К следующему состоянию
            esp_reset_state = ESP_RESET_STATE_BOOT;
            return;
        case ESP_RESET_STATE_BOOT:
            IO_PORT_SET(IO_ESP_CS);                                             // Slave deselect
            // Таймер на ожидание инициализации
            esp_reset_timer.start_us(ESP_START_TIME_US);
            // К следующему состоянию
            esp_reset_state = ESP_RESET_STATE_INIT;
            return;
        case ESP_RESET_STATE_INIT:
            // Запуск таймера опроса
            esp_io_begin_timer.start_hz(ESP_SPI_IPC_TX_HZ, TIMER_FLAG_LOOP);
            // Завершение инициализации
            esp_reset_state = ESP_RESET_STATE_IDLE;
            return;
    }
});

// Сброс чипа
static void esp_reset_do(void)
{
    if (esp_reset_state > ESP_RESET_STATE_IDLE)
        return;
    esp_reset_state = ESP_RESET_STATE_RESET;
    // Остановка таймера опроса
    esp_io_begin_timer.stop();
    // Начало сброса
    IO_PORT_RESET(IO_ESP_CS);                                                   // Slave select
    IO_PORT_RESET(IO_ESP_RST);                                                  // Esp reset
    // Сброс соединения
    esp_link.reset();
    // Таймер на сброс
    esp_reset_timer.start_us(ESP_PIN_SWUTCH_US);
}

void esp_init(void)
{
    // Тактирование и сброс SPI
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;                                         // SPI1 clock enable
    RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;                                      // SPI1 reset
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;                                     // SPI1 unreset
    // Конфигурирование SPI (6 МГц @ MCU 96 МГц)
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_1 | SPI_CR1_BR_0 |                    // SPI disable, CPHA first, CPOL low, Master, Clock /16, MSB, 8-bit, No CRC, Full Duplex
                SPI_CR1_SSM | SPI_CR1_SSI;                                      // SSM on, SSI on
    SPI1->CR2 = SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;                              // DMA TX RX enable
    SPI1->CR1 |= SPI_CR1_SPE;                                                   // SPI1 enable

    // Конфигурирование DMA
    DMA1->IFCR |= DMA_IFCR_CTCIF2;                                              // Clear CTCIF
    // Включаем прерывание канала DMA
    nvic_irq_enable_set(DMA1_Channel2_IRQn, true);
    // Канал 2 (RX)
    esp_dma_channel_init(DMA1_C2, DMA_CCR_TCIE);                                // Transfer complete IRQ enable
    // Канал 3 (TX)
    esp_dma_channel_init(DMA1_C3, DMA_CCR_DIR);                                 // Memory to peripheral
    // Сброс чипа
    esp_reset_do();
}

void esp_handler_add(ipc_handler_t &handler)
{
    esp_handler_host.handler_add(handler);
}

// Получает текущее значение тиков (реализуется платформой)
ipc_handler_t::tick_t ipc_handler_t::tick_get(void)
{
    return mcu_tick_get();
}

// Получает процессор для передачи (реализуется платформой)
ipc_processor_t & ipc_handler_t::transmitter_get(void)
{
    return esp_link;
}

IRQ_ROUTINE
void esp_interrupt_dma(void)
{
    // DMA
    DMA1->IFCR |= DMA_IFCR_CTCIF2;                                              // Clear CTCIF
    DMA1_C2->CCR &= ~DMA_CCR_EN;                                                // Channel disable
    DMA1_C3->CCR &= ~DMA_CCR_EN;                                                // Channel disable
    // SPI
    WAIT_WHILE(SPI1->SR & SPI_SR_BSY);                                          // Wait for idle
    IO_PORT_SET(IO_ESP_CS);                                                     // Slave deselect
    // Event
    esp_io_complete_event.raise();
}
