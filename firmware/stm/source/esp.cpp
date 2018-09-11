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

// Класс работы с ESP
static class esp_t : public ipc_controller_t, public event_base_t
{
    // Счетчик ошибок передачи
    uint32_t corruption_count;
    
    // Ввода/вывод
    class io_t : public event_base_t
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
    protected:
        // Обработчик события начала ввода/вывод
        virtual void notify_event(void)
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
    public:
        // Конструктор по умолчанию
        io_t(esp_t &parent) : esp(parent), active(false)
        { }
        
        // Инициализация каналов DMA
        void init(void) const
        {
            // Канал 2 (RX)
            dma_channel_init(DMA1_C2, DMA_CCR_TCIE);                            // Transfer complete IRQ enable
            // Канал 3 (TX)
            dma_channel_init(DMA1_C3, DMA_CCR_DIR);                             // Memory to peripheral
        }
        
        // Завершение ввода/вывода (возвращает - нужно ли форсировать опрос)
        bool finalize(void)
        {
            active = false;
            // Извлекаем пакет
            esp.packet_input(dma.rx);
            // Определение форсирования
            return dma.rx.dll.fast;
        }
    } io;
    
    // Cброс чипа
    class reset_chip_t : public notify_t
    {
        // Родительский класс
        esp_t &esp;
    protected:
        // Обработчик события начала ввода/вывод
        virtual void notify_event(void)
        {
            IO_PORT_SET(IO_ESP_CS);                                             // Slave deselect
            // Запуск таймера опроса на 10 Гц
            event_timer_start_hz(esp.io, 10, EVENT_TIMER_FLAG_LOOP);
        }
    public:
        // Конструктор по умолчанию
        reset_chip_t(esp_t &parent) : esp(parent)
        { }
        
        // Сброс чипа
        void operator ()(void)
        {
            // Остановка таймера опроса
            event_timer_stop(esp.io);
            // Начало сброса
            IO_PORT_RESET(IO_ESP_CS);                                           // Slave select
            IO_PORT_RESET(IO_ESP_RST);                                          // Esp reset
                // Сброс слотов
                esp.clear_slots();
                // Таймер на ожидание инициализации на 150 мС
                event_timer_start_us(*this, 150000);
            IO_PORT_SET(IO_ESP_RST);                                            // Esp unreset
        }
    } reset_chip;
    
protected:
    // Сброс прикладного уровня
    virtual void reset_layer(ipc_command_flow_t::reason_t reason, bool internal = true)
    {
        ipc_controller_t::reset_layer(reason, internal);
        if (internal && reason == ipc_command_flow_t::REASON_CORRUPTION)
            corruption_count += 2;
    }
    
    // Добавление данных к передаче
    virtual bool transmit_raw(ipc_dir_t dir, ipc_command_t command, const void *source, size_t size)
    {
        auto result = ipc_controller_t::transmit_raw(dir, command, source, size);
        if (result)
            event_add(io);
        return result;
    }
    
    // Обработчик события получения пакета
    virtual void notify_event(void)
    {
        // Завершение ввода/вывода
        auto force = io.finalize();
        // Обработка счетчика ошибок передачи
        if (corruption_count > 0 && --corruption_count >= 10)
        {
            reset_chip();
            return;
        }
        if (!tx_empty())
            force = true;
        // Форсирование передачи
        if (force)
            event_add(io);
    }
public:
    // Конструктор по умолчанию
    esp_t(void) : io(*this), reset_chip(*this), corruption_count(0)
    { }
    
    // Инициализация контроллера
    void init(void)
    {
        // Инициализация каналов DMA
        io.init();
        // Сброс чипаа
        reset_chip();
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

void esp_handler_add_idle(ipc_handler_idle_t &handler)
{
    esp.handler_add_idle(handler);
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
