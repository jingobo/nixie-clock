#include "mcu.h"
#include "led.h"
#include "nvic.h"

// Alias
#define DMA1_C6                 DMA1_Channel6

// ���������� �������� �������
#define LED_RGB_COM_COUNT       3
// ������� � ����� �� �����
#define LED_RGB_BIT_DEPTH       8

// ������ � ����� �������
#define LED_TIMER_PERIOD        90
// ���� ������ � ����� �������� ��������� ����� 
#define LED_HIBIT_PERIOD        (LED_TIMER_PERIOD * 41 / 125)
// ������� ��� ��������� ����
#define LED_LINE_BIT_LOW        (LED_HIBIT_PERIOD)
#define LED_LINE_BIT_HIGH       (LED_HIBIT_PERIOD * 2)

// ������� ������������ ���������
class led_driver_t : public hmi_filter_t<hmi_rgb_t, LED_COUNT>
{
    // ��� ������ ��� ����������
    typedef uint8_t sat_t[LED_RGB_BIT_DEPTH];
    // ��� ������ ��� ���� ��������� ������ ����������
    typedef sat_t rgb_t[LED_RGB_COM_COUNT];
    
    // ���� �� ����������� ������ � ����� DMA
    bool uploaded;
    // ���������� ����� ������ ��� DMA
    ALIGN_FIELD_8
    struct 
    {
        // ��� ������������ �����
        uint16_t reset[22];
        // ������
        rgb_t data[LED_COUNT];
        // ��� �������� CCR � ��������� ����� � 0
        uint8_t gap;
    } dma_buffer;
    ALIGN_FIELD_DEF
protected:
    // ��������� ����� ����������
    virtual void do_data_set(hmi_rank_t index, hmi_rgb_t &data);
    // ���������� ��������� ����������� (�������� �������� ���������� �� 0.25 ��)
    virtual void do_refresh(void);
public:
    // ����������� �� ���������
    led_driver_t(void) : hmi_filter_t(HMI_FILTER_PURPOSE_DISPLAY), uploaded(false)
    { 
        MEMORY_CLEAR(dma_buffer);
    }

    // �������� ��������� �� ����� ��� DMA
    void * dma_pointer_get(void)
    {
        return &dma_buffer;
    }
};

// ������� �������� ��� �����������
led_filter_chain_t led;
// ������ ��������� ����� ��� �����������
led_filter_gamma_t led_filter_gamma;
// ��������� �������� �����������
static led_driver_t led_driver;

void led_driver_t::do_refresh(void)
{
    if (uploaded)
        return;
    uploaded = true;
    // �������� ��������� TIM
    WAIT_WHILE(TIM1->CR1 & TIM_CR1_CEN);
    // ���������� �������� ������ DMA
    rgb_t *dest = dma_buffer.data;
    for (hmi_rank_t led = 0; led < LED_COUNT; led++)
    {
        rgb_t buf;
        hmi_rgb_t color = data_get(led);
        // ������������ � �����
        for (uint8_t i = 0; i < LED_RGB_BIT_DEPTH; i++)
            for (uint8_t j = 0; j < LED_RGB_COM_COUNT; j++)
            {
                buf[j][i] = (color.grba[j] & 0x80) ? LED_LINE_BIT_HIGH : LED_LINE_BIT_LOW;
                color.grba[j] <<= 1;
            }
        // ����������� � ����� DMA
        memcpy(dest++, buf, sizeof(rgb_t));
    }
    // ����� �������
    TIM1->CR1 &= ~TIM_CR1_OPM;                                                  // OPM disable
    TIM1->CR1 |= TIM_CR1_CEN;                                                   // TIM enable
    // ����� DMA
    DMA1_C6->CNDTR = sizeof(dma_buffer);                                        // Transfer data size
    DMA1_C6->CCR |= DMA_CCR_EN;                                                 // Channel enable
}

void led_driver_t::do_data_set(hmi_rank_t index, hmi_rgb_t &data)
{
    hmi_filter_t::do_data_set(index, data);
    uploaded = false;
}

void led_filter_gamma_t::do_data_set(hmi_rank_t index, hmi_rgb_t &data)
{
    data.gamma(table);
}

void led_init(void)
{
    // ������������ � ����� �������
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;                                         // TIM1 clock enable
    RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST;                                      // TIM1 reset
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;                                     // TIM1 unreset
    // ���������������� �������
    TIM1->CR1 = TIM_CR1_ARPE;                                                   // TIM disable, UEV on, OPM off, Up, CMS edge, Clock /1, ARR preloaded
    TIM1->DIER = TIM_DIER_CC3DE;                                                // CC3 DMA request enable
    TIM1->CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;        // CC3 output, CC3 Fast off, CC3 preloaded, CC3 mode PWM 1 (110)
    TIM1->CCER = TIM_CCER_CC3E;                                                 // CC3 output enable, CC3 Polarity high
    TIM1->ARR = LED_TIMER_PERIOD - 1;                                           // Period 800 kHz (1.25 uS)
    TIM1->BDTR = TIM_BDTR_MOE;                                                  // Main Output enable
    
    // ���������������� DMA
    DMA1->IFCR |= DMA_IFCR_CTCIF6;                                              // Clear CTCIF
    // ����� 6
    mcu_dma_channel_setup_pm(DMA1_C6,
        // � TIM CC3
        TIM1->CCR3, 
        // �� ������
        led_driver.dma_pointer_get());
    DMA1_C6->CCR |= DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 |              // Memory to peripheral, Memory increment (8-bit), Peripheral size 16-bit
                    DMA_CCR_PL | DMA_CCR_TCIE;                                  // Very high priority, Transfer complete interrupt enable
    // �������� ���������� ������ DMA
    nvic_irq_enable_set(DMA1_Channel6_IRQn, true);
    // ��������� � ������� �������
    led.attach(led_driver);
    // ���������� �����������
    led.refresh();
}

IRQ_ROUTINE
void led_interrupt_dma(void)
{
    TIM1->CR1 |= TIM_CR1_OPM;                                                   // OPM enable
    DMA1_C6->CCR &= ~DMA_CCR_EN;                                                // Channel disable
    DMA1->IFCR |= DMA_IFCR_CTCIF6;                                              // Clear CTCIF
}
