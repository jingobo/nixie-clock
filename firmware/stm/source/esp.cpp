#include "io.h"
#include "esp.h"
#include "mcu.h"
#include "nvic.h"
#include "event.h"

// Alias
#define DMA1_C2                 DMA1_Channel2
#define DMA1_C3                 DMA1_Channel3

// ������� ESP8266 TX & RX ������������ ����������
#define ESP_SPI_CMD_RD_WR       0x06

// ����� ������ � ESP
static class esp_t : public ipc_controller_t, public event_base_t
{
    // ������� ������ ��������
    uint32_t corruption_count;
    
    // �����/�����
    class io_t : public event_base_t
    {
        // ����� ������ ��� DMA
        ALIGN_FIELD_8
        struct
        {
            // ��� ������������ � ������� ������
            uint8_t pads[3];
            struct
            {
                // ���� ������� ��� ESP8266
                uint8_t command;
                // ����� ����� � ��������
                ipc_packet_t tx, rx;
            } dma;
        };
        ALIGN_FIELD_DEF
        // ������������ �����
        esp_t &esp;
        // ���� ���������� �����������
        bool active;
        
        // ������������� ������ DMA
        void dma_channel_init(DMA_Channel_TypeDef *channel, uint32_t flags) const
        {
            // ���������������� DMA
            mcu_dma_channel_setup_pm(channel,
                // �� SPI
                SPI1->DR, 
                // � ������
                &dma);
            channel->CCR |= DMA_CCR_MINC | DMA_CCR_PL_1 | flags;                // Direction (flags), Memory increment (8-bit), Peripheral size 8-bit, High priority
        }
    protected:
        // ���������� ������� ������ �����/�����
        virtual void notify_event(void)
        {
            if (active)
                return;
            active = true;
            // ���������� ������
            packet_clear(dma.rx);
            esp.packet_output(dma.tx);
            dma.command = ESP_SPI_CMD_RD_WR;
            // ������ ��������
            IO_PORT_RESET(IO_ESP_CS);                                           // Slave select
            // DMA
            DMA1_C2->CNDTR = DMA1_C3->CNDTR = sizeof(dma);                      // Transfer data size
            DMA1_C2->CCR |= DMA_CCR_EN;                                         // Channel enable
            DMA1_C3->CCR |= DMA_CCR_EN;                                         // Channel enable
        }
    public:
        // ����������� �� ���������
        io_t(esp_t &parent) : esp(parent), active(false)
        { }
        
        // ������������� ������� DMA
        void init(void) const
        {
            // ����� 2 (RX)
            dma_channel_init(DMA1_C2, DMA_CCR_TCIE);                            // Transfer complete IRQ enable
            // ����� 3 (TX)
            dma_channel_init(DMA1_C3, DMA_CCR_DIR);                             // Memory to peripheral
        }
        
        // ���������� �����/������ (���������� - ����� �� ����������� �����)
        bool finalize(void)
        {
            active = false;
            // ��������� �����
            esp.packet_input(dma.rx);
            // ����������� ������������
            return dma.rx.dll.fast;
        }
    } io;
    
    // C���� ����
    class reset_chip_t : public notify_t
    {
        // ������������ �����
        esp_t &esp;
    protected:
        // ���������� ������� ������ �����/�����
        virtual void notify_event(void)
        {
            IO_PORT_SET(IO_ESP_CS);                                             // Slave deselect
            // ������ ������� ������ �� 10 ��
            event_timer_start_hz(esp.io, 10, EVENT_TIMER_FLAG_LOOP);
        }
    public:
        // ����������� �� ���������
        reset_chip_t(esp_t &parent) : esp(parent)
        { }
        
        // ����� ����
        void operator ()(void)
        {
            // ��������� ������� ������
            event_timer_stop(esp.io);
            // ������ ������
            IO_PORT_RESET(IO_ESP_CS);                                           // Slave select
            IO_PORT_RESET(IO_ESP_RST);                                          // Esp reset
                // ����� ������
                esp.clear_slots();
                // ������ �� �������� ������������� �� 150 ��
                event_timer_start_us(*this, 150000);
            IO_PORT_SET(IO_ESP_RST);                                            // Esp unreset
        }
    } reset_chip;
    
protected:
    // ����� ����������� ������
    virtual void reset_layer(ipc_command_flow_t::reason_t reason, bool internal = true)
    {
        ipc_controller_t::reset_layer(reason, internal);
        if (internal && reason == ipc_command_flow_t::REASON_CORRUPTION)
            corruption_count += 2;
    }
    
    // ���������� ������ � ��������
    virtual bool transmit_raw(ipc_dir_t dir, ipc_command_t command, const void *source, size_t size)
    {
        auto result = ipc_controller_t::transmit_raw(dir, command, source, size);
        if (result)
            event_add(io);
        return result;
    }
    
    // ���������� ������� ��������� ������
    virtual void notify_event(void)
    {
        // ���������� �����/������
        auto force = io.finalize();
        // ��������� �������� ������ ��������
        if (corruption_count > 0 && --corruption_count >= 10)
        {
            reset_chip();
            return;
        }
        if (!tx_empty())
            force = true;
        // ������������ ��������
        if (force)
            event_add(io);
    }
public:
    // ����������� �� ���������
    esp_t(void) : io(*this), reset_chip(*this), corruption_count(0)
    { }
    
    // ������������� �����������
    void init(void)
    {
        // ������������� ������� DMA
        io.init();
        // ����� �����
        reset_chip();
    }    
} esp;

void esp_init(void)
{
    IPC_PKT_SIZE_CHECK();
    // ������������ � ����� SPI
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;                                         // SPI1 clock enable
    RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;                                      // SPI1 reset
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;                                     // SPI1 unreset
    // ���������������� SPI (6 ��� @ MCU 96 ���)
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_1 | SPI_CR1_BR_0 |                    // SPI disable, CPHA first, CPOL low, Master, Clock /16, MSB, 8-bit, No CRC, Full Duplex
                SPI_CR1_SSM | SPI_CR1_SSI;                                      // SSM on, SSI on
    SPI1->CR2 = SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;                              // DMA TX RX enable
    SPI1->CR1 |= SPI_CR1_SPE;                                                   // SPI1 enable

    // ���������������� DMA
    DMA1->IFCR |= DMA_IFCR_CTCIF2;                                              // Clear CTCIF
    // �������� ���������� ������ DMA
    nvic_irq_enable_set(DMA1_Channel2_IRQn, true);
    // ������������� �����������
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
