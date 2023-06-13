#include "io.h"
#include "neon.h"
#include "timer.h"
#include "screen.h"
#include "system.h"

// Смещение насыщенности для сброс PWM после UEV
constexpr const hmi_sat_t NEON_SAT_DX = 1;
// Частота мультиплексирования неонок [Гц]
constexpr const uint32_t NEON_MUX_HZ = HMI_FRAME_RATE * NEON_COUNT;

// Сброс адресной линии анодов
static void neon_sel_reset(void)
{
    IO_PORT_RESET_MASK(IO_SSEL0_PORT, 
        IO_MASK(IO_SSEL0) | 
        IO_MASK(IO_SSEL1));
}

// Драйвер вывода неонок
static class neon_display_t : public neon_model_t::display_t
{
    // Данные неонки (прерывание)
    struct irq_t
    {
        // Ширина импульса
        uint8_t pw = 0;
    } irq[NEON_COUNT];

    // Данные лампы (вывод)
    struct out_t
    {
        // Для определения обновления
        bool unchanged = false;
    } out[NEON_COUNT];
    
    // Индекс выводимой дампы (мультиплексирование)
    uint8_t nmi = 0;
protected:
    // Обработчик изменения данных
    virtual void data_changed(hmi_rank_t index, neon_data_t &data) override final
    {
        // Установка данных
        out[index].unchanged = false;
    }
    
    // Обновление состояния неонок
    virtual void refresh(void) override final
    {
        // Базовый метод
        display_t::refresh();
        
        // Пересчет данных прерываний
        for (hmi_rank_t i = 0; i < NEON_COUNT; i++)
        {
            if (out[i].unchanged)
                continue;
            out[i].unchanged = true;
            
            // Пересчет ширины импульса из насыщенности
            const auto pw = HMI_SAT_MAX - HMI_GAMMA_TABLE[in_get(i).sat];
            
            // Перенос данных с запретом прерываний
            IRQ_SAFE_ENTER();
                irq[i].pw = pw;
            IRQ_SAFE_LEAVE();
        }
    }
public:
    // Мультиплексирование
    void mux(void)
    {
        // Установка анодного напряжения
        neon_sel_reset();
        IO_PORT_SET_MASK(IO_SSEL0_PORT, MASK_32(nmi, IO_SSEL0));

        // PWM
        TIM2->CNT = (irq[nmi].pw >> 1) + NEON_SAT_DX;                           // Update counter (center aligned)
        TIM2->CCR3 = irq[nmi].pw + NEON_SAT_DX;                                 // Update CC3 value
        TIM2->CR1 |= TIM_CR1_CEN;                                               // TIM enable

        // Переход к следующей неонке
        if (++nmi >= NEON_COUNT)
            nmi = 0;
    }
} neon_display;

// Таймер мультиплексирования неонок
static timer_callback_t neon_mux_timer([](void)
{
    neon_display.mux();
});

void neon_init(void)
{
    // Сброс адресных линий
    neon_sel_reset();
    
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
    screen.neon.attach(neon_display);
    // Выставление задачи мультиплексирования
    neon_mux_timer.start_hz(NEON_MUX_HZ, TIMER_PRI_CRITICAL | TIMER_FLAG_LOOP);
}
