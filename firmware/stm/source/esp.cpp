#include "io.h"
#include "esp.h"
#include "mcu.h"
#include "nvic.h"
#include "event.h"

// Alias
#define DMA1_C2                 DMA1_Channel2
#define DMA1_C3                 DMA1_Channel3

// Команда ESP8266 TX & RX подчиненного устройства
#define ESP_SPI_CMD_RD_WR       0x06
// Частота передачи пакетов IPC по SPI
#define ESP_SPI_IPC_TX_HZ       100
// Время ожидания после смены состояния выводов ESP8266
#define ESP_PIN_SWUTCH_US       XK(50)
// Время ожидания инициализации чипа ESP8266
#define ESP_START_TIME_US       XK(300)

// Класс работы с ESP
static class esp_t : public ipc_controller_master_t, public event_base_t
{    
    // Ввода/вывод
    class io_t : public notify_t
    {
        // Буфер пакета для DMA
        ALIGN_FIELD_8
        struct
        {
            // Для выравнивания к четному адресу
            uint8_t pads[3];
            struct
            {
                // Байт команды для ESP8266
                uint8_t command;
                // Пакет приёма и передачи
                ipc_packet_t tx, rx;
            } dma;
        };
        ALIGN_FIELD_DEF
        // Родительский класс
        esp_t &esp;
        // Флаг активности транзакцими
        bool active;
        
        // Инициализация канала DMA
        void dma_channel_init(DMA_Channel_TypeDef *channel, uint32_t flags) const
        {
            // Конфигурирование DMA
            mcu_dma_channel_setup_pm(channel,
                // Из SPI
                SPI1->DR, 
                // В память
                &dma);
            channel->CCR |= DMA_CCR_MINC | DMA_CCR_PL_1 | flags;                // Direction (flags), Memory increment (8-bit), Peripheral size 8-bit, High priority
        }
    public:
        // Конструктор по умолчанию
        io_t(esp_t &parent) : esp(parent), active(false)
        { }

        // Обработчик события начала ввода/вывод
        virtual void notify(void)
        {
            if (active)
                return;
            active = true;
            // Подготовка данных
            packet_clear(dma.rx);
            esp.packet_output(dma.tx);
            dma.command = ESP_SPI_CMD_RD_WR;
            // Начало передачи
            IO_PORT_RESET(IO_ESP_CS);                                           // Slave select
            // DMA
            DMA1_C2->CNDTR = DMA1_C3->CNDTR = sizeof(dma);                      // Transfer data size
            DMA1_C2->CCR |= DMA_CCR_EN;                                         // Channel enable
            DMA1_C3->CCR |= DMA_CCR_EN;                                         // Channel enable
        }
        
        // Инициализация каналов DMA
        void init(void) const
        {
            // Канал 2 (RX)
            dma_channel_init(DMA1_C2, DMA_CCR_TCIE);                            // Transfer complete IRQ enable
            // Канал 3 (TX)
            dma_channel_init(DMA1_C3, DMA_CCR_DIR);                             // Memory to peripheral
        }
        
        // Завершение ввода/вывода (возвращает - нужно ли форсировать опрос)
        void finalize(void)
        {
            active = false;
            // Извлекаем пакет
            esp.packet_input(dma.rx);
        }
    } io;
    
    // Cброс чипа
    class reset_chip_t : public notify_t
    {
        // Родительский класс
        esp_t &esp;
        // Состояние сброса
        enum
        {
            // Сброс не происходит
            STATE_IDLE = 0,
            // Сброс (RST на землю)
            STATE_RESET,
            // Загрузка (BOOT0 к питанию)
            STATE_BOOT,
            // Ожидание инициализации
            STATE_INIT
        } state;
    public:
        // Конструктор по умолчанию
        reset_chip_t(esp_t &parent) : esp(parent), state(STATE_IDLE)
        { }

        // Обработчик события начала ввода/вывод
        virtual void notify(void)
        {
            switch (state)
            {
                case STATE_RESET:
                    IO_PORT_SET(IO_ESP_RST);                                    // Esp unreset
                    // Таймер на бутлоадер
                    event_timer_start_us(*this, ESP_PIN_SWUTCH_US);
                    // К следующему состоянию
                    state = STATE_BOOT;
                    return;
                case STATE_BOOT:
                    IO_PORT_SET(IO_ESP_CS);                                     // Slave deselect
                    // Таймер на ожидание инициализации
                    event_timer_start_us(*this, ESP_START_TIME_US);
                    // К следующему состоянию
                    state = STATE_INIT;
                    return;
                case STATE_INIT:
                    // Запуск таймера опроса
                    event_timer_start_hz(esp.io, ESP_SPI_IPC_TX_HZ, EVENT_TIMER_FLAG_LOOP);
                    // Завершение инициализации
                    state = STATE_IDLE;
                    return;
            }
        }
        
        // Сброс чипа
        void operator ()(void)
        {
            if (state > STATE_IDLE)
                return;
            state = STATE_RESET;
            // Остановка таймера опроса
            event_timer_stop(esp.io);
            // Начало сброса
            IO_PORT_RESET(IO_ESP_CS);                                           // Slave select
            IO_PORT_RESET(IO_ESP_RST);                                          // Esp reset
            // Сброс слотов
            esp.clear_slots();
            // Таймер на сброс
            event_timer_start_us(*this, ESP_PIN_SWUTCH_US);
        }
    } reset_chip;
protected:
    // Массовый сброс (другая сторона не отвечает)
    virtual void reset_total(void)
    {
        // Сброс чипаа
        reset_chip();
    }
public:
    // Конструктор по умолчанию
    esp_t(void) : io(*this), reset_chip(*this)
    { }
    
    // Инициализация контроллера
    void init(void)
    {
        // Инициализация каналов DMA
        io.init();
        // Сброс чипаа
        reset_chip();
    }    

    // Обработчик события получения пакета
    virtual void notify(void)
    {
        // Завершение ввода/вывода
        io.finalize();
    }
} esp;

void esp_init(void)
{
    IPC_PKT_SIZE_CHECK();
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
    // Инициализация контроллера
    esp.init();
}

void esp_handler_add_command(ipc_handler_command_t &handler)
{
    esp.handler_add_command(handler);
}

void esp_handler_add_event(ipc_handler_event_t &handler)
{
    esp.handler_add_event(handler);
}

bool esp_transmit(ipc_dir_t dir, const ipc_command_data_t &data)
{
    return esp.transmit(dir, data);
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
    event_add(esp);
}
