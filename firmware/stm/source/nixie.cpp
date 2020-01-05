#include "io.h"
#include "timer.h"
#include "nixie.h"
#include "screen.h"
#include "system.h"

// Смещение насыщенности для сброс PWM после UEV
#define NIXIE_SAT_DX                    1
// Частота мультиплексирования ламп [Гц]
#define NIXIE_MUX_HZ                    (HMI_FRAME_RATE * NIXIE_COUNT)

// Сброс адресной линии анодов
#define NIXIE_SELA_RESET()              \
    IO_PORT_RESET_MASK(IO_TSELA0_PORT,  \
        IO_MASK(IO_TSELA0) |            \
        IO_MASK(IO_TSELA1) |            \
        IO_MASK(IO_TSELA2))

// Установка адресной линии анодов
#define NIXIE_SELA_SET(v)               \
    NIXIE_SELA_RESET();                 \
    IO_PORT_SET_MASK(IO_TSELA0_PORT, MASK_32(v, IO_TSELA0))

// Сброс адресной линии катодов
#define NIXIE_SELC_RESET()              \
    IO_PORT_RESET_MASK(IO_TSELP_PORT,   \
        IO_MASK(IO_TSELP) |             \
        IO_MASK(IO_TSELC0) |            \
        IO_MASK(IO_TSELC1) |            \
        IO_MASK(IO_TSELC2) |            \
        IO_MASK(IO_TSELC3))

// Установка адресной линии катодов
#define NIXIE_SELC_SET(v)               \
    NIXIE_SELC_RESET();                 \
    IO_PORT_SET_MASK(IO_TSELC0_PORT, MASK_32(v, IO_TSELP))

// Подсчет значения адресной линии катодов
#define NIXIE_SELC_CALC(v, dot)         \
    (MASK(uint8_t, v, 1) | (dot ? 1 : 0))

// Таблица перевода реальной цифры в цифру полученную после трассировки
static const uint8_t NIXIE_PIN_FIX[11] =
{
    8, 3, 1, 6, 9, 0, 4, 2, 7, 5, 10
};

// Развязочная таблица, указаны цифры по убыванию в глубь лампы
uint8_t NIXIE_DIGITS_BY_PLACES[10] = 
{ 
    3, 8, 9, 4, 0, 5, 7, 2, 6, 1,
};

// Развязочная таблица, указаны места по возрастанию цифры
uint8_t NIXIE_PLACES_BY_DIGITS[10] = 
{
    4, 9, 7, 0, 3, 5, 8, 6, 1, 2,
};

// Драйвер вывода ламп
static class nixie_driver_t : public nixie_filter_t
{
    // Данные лампы (прерывание)
    struct irq_t
    {
        // Ширина импульса
        uint8_t pw;
        // Адрес катода
        uint8_t selc;
        
        // Конструктор по умолчанию
        irq_t(void) : pw(0), selc(NIXIE_SELC_CALC(NIXIE_DIGIT_SPACE, false))
        { }
    } irq[NIXIE_COUNT];

    // Данные лампы (вывод)
    struct out_t
    {
        // Парметры
        nixie_data_t data;
        // Для определения обновления
        bool unchanged;
        
        // Конструктор по умолчанию
        out_t(void) : unchanged(false)
        { }
    } out[NIXIE_COUNT];
    // Индекс выводимой дампы (мультиплексирование)
    uint8_t nmi;
protected:
    // Событие обработки новых данных
    virtual void process(hmi_rank_t index, nixie_data_t &data) override final
    {
        // Установка данных
        out[index].data = data;
        out[index].unchanged = false;
        // Базовый метод (не вызываем, потому что это дисплей)
        // nixie_filter_t::process(index, data);
    }
    
    // Обновление состояния ламп
    OPTIMIZE_SPEED
    virtual void refresh(void) override final
    {
        // Базовый метод
        nixie_filter_t::refresh();
        // Пересчет данных прерываний
        for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
        {
            if (out[i].unchanged)
                continue;
            out[i].unchanged = true;
            // Проверка цифры
            assert(out[i].data.digit <= NIXIE_DIGIT_SPACE);
            // Формирование цифры
            irq[i].selc = NIXIE_SELC_CALC(NIXIE_PIN_FIX[out[i].data.digit], out[i].data.dot);
            // Формирование ширины импульса по насыщенности
            hmi_sat_t sat = (out[i].data.digit != NIXIE_DIGIT_SPACE) ?
                out[i].data.sat :
                HMI_SAT_MIN;
            irq[i].pw = HMI_SAT_MAX - HMI_GAMMA_TABLE[sat];
        }
    }
public:
    // Конструктор по умолчанию
    nixie_driver_t(void) : nixie_filter_t(HMI_FILTER_PURPOSE_DISPLAY), nmi(0)
    { }
    
    // Мультиплексирование
    OPTIMIZE_SPEED
    void mux(void)
    {
        // Переключение катодного напряжения
        NIXIE_SELC_SET(irq[nmi].selc);
        // Установка анодного напряжения
        NIXIE_SELA_SET(nmi);
        
        // PWM
        TIM4->CNT = (irq[nmi].pw >> 1) + NIXIE_SAT_DX;                          // Update counter (center aligned)
        TIM4->CCR2 = irq[nmi].pw + NIXIE_SAT_DX;                                // Update CC2 value
        TIM4->CR1 |= TIM_CR1_CEN;                                               // TIM enable

        // Переход к следующей лампе
        if (++nmi >= NIXIE_COUNT)
            nmi = 0;
    }
} nixie_driver;

// Таймер мультиплексирования ламп
static timer_callback_t nixie_mux_timer([](void)
{
    nixie_driver.mux();
});

void nixie_init(void)
{
    // Сброс адресных линий
    NIXIE_SELA_RESET();
    NIXIE_SELC_RESET();
    // Тактирование и сброс таймера
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;                                         // TIM clock enable
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM4RST;                                      // TIM reset
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM4RST;                                     // TIM unreset
    // Базовое конфигурирование таймера без PWM
    TIM4->CR1 = TIM_CR1_OPM;                                                    // TIM disable, UEV on, OPM on, Up, CMS edge, Clock /1, ARR preload off
    TIM4->PSC = FMCU_NORMAL_HZ / (NIXIE_MUX_HZ * (HMI_SAT_COUNT + 1)) - 1;      // Prescaler (frequency)
    TIM4->ARR = HMI_SAT_MAX + NIXIE_SAT_DX - 1;                                 // PWM Period
    TIM4->BDTR = TIM_BDTR_MOE;                                                  // Main output enable
    // Настройка канала 2
    TIM4->CCR2 = HMI_SAT_MAX;                                                   // Update CC2 value (prevent initial output high state)
    TIM4->CCMR1 = TIM_CCMR1_OC2M_0 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;       // CC2 output, CC2 Fast off, CC2 preload off, CC2 mode PWM 2 (111)
    TIM4->CCER = TIM_CCER_CC2E;                                                 // CC2 output enable, CC2 Polarity high
    // Добавление дисплея в цепочку
    screen.nixie.attach(nixie_driver);
    // Выставление задачи мультиплексирования
    nixie_mux_timer.start_hz(NIXIE_MUX_HZ, TIMER_PRI_CRITICAL | TIMER_FLAG_LOOP);
}
