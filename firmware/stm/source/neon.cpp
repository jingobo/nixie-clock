#include "io.h"
#include "neon.h"
#include "timer.h"
#include "screen.h"
#include "system.h"

// Смещение насыщенности для сброс PWM после UEV
#define NEON_SAT_DX                     1
// Частота мультиплексирования неонок [Гц]
#define NEON_MUX_HZ                     (HMI_FRAME_RATE * NEON_COUNT)
        
// Сброс адресной линии анодов
#define NEON_SEL_RESET()                \
    IO_PORT_RESET_MASK(IO_SSEL0_PORT,   \
        IO_MASK(IO_SSEL0) |             \
        IO_MASK(IO_SSEL1))

// Установка адресной линии анодов
#define NEON_SEL_SET(v)                 \
    NEON_SEL_RESET();                   \
    IO_PORT_SET_MASK(IO_SSEL0_PORT, MASK_32(v, IO_SSEL0))

// Драйвер вывода неонок
static class neon_driver_t : public neon_filter_t
{
    // Данные неонки (прерывание)
    struct irq_t
    {
        // Ширина импульса
        uint8_t pw;
        
        // Конструктор по умолчанию
        irq_t(void) : pw(0)
        { }
    } irq[NEON_COUNT];

    // Данные лампы (вывод)
    struct out_t
    {
        // Параметры отображения
        neon_data_t data;
        // Для определения обновления
        bool unchanged;
        
        // Конструктор по умолчанию
        out_t(void) : data(HMI_SAT_MIN), unchanged(false)
        { }
    } out[NEON_COUNT];
    // Индекс выводимой дампы (мультиплексирование)
    uint8_t nmi;
protected:
    // Событие обработки новых данных
    virtual void process(hmi_rank_t index, neon_data_t &data) override final
    {
        // Установка данных
        out[index].data = data;
        out[index].unchanged = false;
        // Базовый метод (не вызываем, потому что это дисплей)
        // neon_filter_t::process(index, data);
    }
    
    // Обновление состояния неонок
    OPTIMIZE_SPEED
    virtual void refresh(void) override final
    {
        // Базовый метод
        neon_filter_t::refresh();
        // Пересчет данных прерываний
        for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
        {
            if (out[i].unchanged)
                continue;
            out[i].unchanged = true;
            // Пересчет ширины импульса из насыщенности
            irq[i].pw = HMI_SAT_MAX - HMI_GAMMA_TABLE[out[i].data.sat];
        }
    }
public:
    // Конструктор по умолчанию
    neon_driver_t(void) : neon_filter_t(HMI_FILTER_PURPOSE_DISPLAY), nmi(0)
    { }
    
    // Мультиплексирование
    OPTIMIZE_SPEED
    void mux(void)
    {
        // Установка анодного напряжения
        NEON_SEL_SET(nmi);

        // PWM
        TIM2->CNT = (irq[nmi].pw >> 1) + NEON_SAT_DX;                           // Update counter (center aligned)
        TIM2->CCR3 = irq[nmi].pw + NEON_SAT_DX;                                 // Update CC3 value
        TIM2->CR1 |= TIM_CR1_CEN;                                               // TIM enable

        // Переход к следующей неонке
        if (++nmi >= NEON_COUNT)
            nmi = 0;
    }
} neon_driver;

// Таймер мультиплексирования неонок
static timer_callback_t neon_mux_timer([](void)
{
    neon_driver.mux();
});

void neon_init(void)
{
    // Сброс адресных линий
    NEON_SEL_RESET();
    // Тактирование и сброс таймера
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;                                         // TIM clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST;                                      // TIM reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;                                     // TIM unreset
    // Базовое конфигурирование таймера без PWM
    TIM2->CR1 = TIM_CR1_OPM;                                                    // TIM disable, UEV on, OPM on, Up, CMS edge, Clock /1, ARR preload off
    TIM2->PSC = FMCU_NORMAL_HZ / (NEON_MUX_HZ * (HMI_SAT_COUNT + 1)) - 1;       // Prescaler (frequency)
    TIM2->ARR = HMI_SAT_MAX + NEON_SAT_DX - 1;                                  // PWM Period
    TIM2->BDTR = TIM_BDTR_MOE;                                                  // Main Output enable
    // Настройка канала 3
    TIM2->CCR3 = HMI_SAT_MAX;                                                   // Update CC3 value (prevent initial output high state)
    TIM2->CCMR2 = TIM_CCMR2_OC3M_0 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;       // CC3 output, CC3 Fast off, CC3 preload off, CC3 mode PWM 2 (111)
    TIM2->CCER = TIM_CCER_CC3E;                                                 // CC3 output enable, CC3 Polarity high
    // Добавление дисплея в цепочку
    screen.neon.attach(neon_driver);
    // Выставление задачи мультиплексирования
    neon_mux_timer.start_hz(NEON_MUX_HZ, TIMER_PRI_CRITICAL | TIMER_FLAG_LOOP);
}
