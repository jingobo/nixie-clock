#include "io.h"
#include "tube.h"
#include "nvic.h"
#include "event.h"
#include "system.h"

// Смещение насыщенности для сброс PWM после UEV
#define TUBE_SAT_DX                     1
// Частота мультиплексирования ламп [Гц]
#define TUBE_MUX_HZ                     (HMI_FRAME_RATE * TUBE_NIXIE_COUNT)

// Сброс адресной линии анодов
#define TUBE_NIXIE_SELA_RESET()         \
    IO_PORT_RESET_MASK(IO_TSELA0_PORT,  \
        IO_MASK(IO_TSELA0) |            \
        IO_MASK(IO_TSELA1) |            \
        IO_MASK(IO_TSELA2))

// Установка адресной линии анодов
#define TUBE_NIXIE_SELA_SET(v)          \
    TUBE_NIXIE_SELA_RESET();            \
    IO_PORT_SET_MASK(IO_TSELA0_PORT, MASK_32(v, IO_TSELA0))

// Сброс адресной линии катодов
#define TUBE_NIXIE_SELС_RESET()         \
    IO_PORT_RESET_MASK(IO_TSELP_PORT,   \
        IO_MASK(IO_TSELP) |             \
        IO_MASK(IO_TSELC0) |            \
        IO_MASK(IO_TSELC1) |            \
        IO_MASK(IO_TSELC2) |            \
        IO_MASK(IO_TSELC3))

// Установка адресной линии катодов
#define TUBE_NIXIE_SELC_SET(v)          \
    TUBE_NIXIE_SELС_RESET();            \
    IO_PORT_SET_MASK(IO_TSELC0_PORT, MASK_32(v, IO_TSELP))

// Подсчет значения адресной линии катодов
#define TUBE_NIXIE_SELC_CALC(v, dot)    \
    (MASK(uint8_t, v, 1) | (dot ? 1 : 0))
        
// Сброс адресной линии анодов
#define TUBE_NEON_SEL_RESET()           \
    IO_PORT_RESET_MASK(IO_SSEL0_PORT,   \
        IO_MASK(IO_SSEL0) |             \
        IO_MASK(IO_SSEL1))

// Установка адресной линии анодов
#define TUBE_NEON_SEL_SET(v)            \
    TUBE_NEON_SEL_RESET();              \
    IO_PORT_SET_MASK(IO_SSEL0_PORT, MASK_32(v, IO_SSEL0))
        
// Базовая структура данных дамп
typedef struct
{
    // Насыщенность
    hmi_sat_t sat;
} tube_data_t;

// Данные цифровой лампы (прерывание)
typedef struct : tube_data_t
{
    // Отображаемая цифра
    uint8_t digit;
} tube_nixie_irq_t;

// Данные цифровой лампы
typedef struct : tube_nixie_irq_t
{
    // Состояние точки
    bool dot : 1;
    // Для определения обновления
    bool unchanged : 1;
} tube_nixie_t;

// Данные неоновой лампы (прерывание)
typedef tube_data_t tube_neon_irq_t;

// Данные неоновой лампы 
typedef struct : tube_neon_irq_t
{
    // Для определения обновления
    bool unchanged : 1;
} tube_neon_t;

// Обобщенный тип ОЗУ данных
template <hmi_rank_t count, typename N, typename R>
class tube_ram_s
{
public:
    // Данные для отображения
    N data[count];
    // Данные для отображения (прерывание)
    R raw[count];
    // Индекс для мультиплексирования
    hmi_rank_t mux;
    
    // Инкремент мультиплексирования
    INLINE_FORCED
    void mux_next(void)
    {
        if (++mux >= count)
            mux = 0;
    }
};

// ОЗУ данные модуля
static __no_init struct
{
    // Цифровые лампы
    tube_ram_s<TUBE_NIXIE_COUNT, tube_nixie_t, tube_nixie_irq_t> nixie;
    // Неоновые лампы
    tube_ram_s<TUBE_NEON_COUNT, tube_neon_t, tube_neon_irq_t> neon;
} tube_ram;

// Таблица гамма корекции
static __no_init hmi_sat_table_t tube_gamma;

// Таблица перевода реальной цифры в цифру полученную после трассировки
static const uint8_t TUBE_NIXIE_PIN_FIX[11] =
{ 8, 3, 1, 6, 9, 0, 4, 2, 7, 5, 10 };

// Переинициализация таймера ШИМ
#define TUBE_PWM_SET(TIM, V, CH)                                                    \
    sat = (V);                                                                      \
    (TIM)->CNT = (sat >> 1) + TUBE_SAT_DX;  /* Update counter (center aligned) */   \
    (TIM)->CCR##CH = sat + TUBE_SAT_DX;     /* Update CCx value */                  \
    (TIM)->CR1 |= TIM_CR1_CEN               /* TIM enable */

// Мультиплексирование
OPTIMIZE_SPEED
static void tube_mux(void)
{
    hmi_sat_t sat;
    // Переключение катодного напряжения (лампы)
    TUBE_NIXIE_SELC_SET(tube_ram.nixie.raw[tube_ram.nixie.mux].digit);
    // Установка анодного напряжения (лампы)
    TUBE_NIXIE_SELA_SET(tube_ram.nixie.mux);
    // Установка анодного напряжения (неонки)
    TUBE_NEON_SEL_SET(tube_ram.neon.mux);

    // PWM (лампы)
    TUBE_PWM_SET(TIM4, tube_ram.nixie.raw[tube_ram.nixie.mux].sat, 2);
    // PWM (неонки)
    TUBE_PWM_SET(TIM2, tube_ram.neon.raw[tube_ram.neon.mux].sat, 3);

    // Переход к следующей лампе
    tube_ram.nixie.mux_next();
    tube_ram.neon.mux_next();
}

// Базовая инициализация аппаратного таймера под ШИМ
static void tube_init_timer(TIM_TypeDef *TIM, uint32_t reset_flag, uint32_t clock_flag)
{
    // Тактирование и сброс таймера
    RCC->APB1ENR |= clock_flag;                                                 // TIM clock enable
    RCC->APB1RSTR |= clock_flag;                                                // TIM reset
    RCC->APB1RSTR &= ~clock_flag;                                               // TIM unreset
    // Базовое конфигурирование таймера без PWM
    TIM->CR1 = TIM_CR1_OPM;                                                     // TIM disable, UEV on, OPM on, Up, CMS edge, Clock /1, ARR preload off
    TIM->PSC = FMCU_NORMAL_HZ / (TUBE_MUX_HZ * (HMI_SAT_COUNT + 1)) - 1;        // Prescaler (frequency)
    TIM->ARR = HMI_SAT_MAX + TUBE_SAT_DX - 1;                                   // PWM Period
    TIM->BDTR = TIM_BDTR_MOE;                                                   // Main Output enable
}

// Инициализация цифровых ламп
INLINE_FORCED
static void tube_init_nixie(void)
{
    // Сброс адресных линий
    TUBE_NIXIE_SELA_RESET();
    TUBE_NIXIE_SELС_RESET();
    // Таймер ШИМ
    tube_init_timer(TIM4, RCC_APB1RSTR_TIM4RST, RCC_APB1ENR_TIM4EN);
    // Настройка канала 2
    TIM4->CCMR1 = TIM_CCMR1_OC2M_0 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;       // CC2 output, CC2 Fast off, CC2 preload off, CC2 mode PWM 2 (111)
    TIM4->CCER = TIM_CCER_CC2E;                                                 // CC2 output enable, CC2 Polarity high
    // TODO: выпилить
    //TIM4->DIER = TIM_DIER_UIE;                                                  // UEV interrupt enable
    //nvic_irq_enable_set(TIM4_IRQn, true);
}

// Инициализация неоновых ламп
INLINE_FORCED
static void tube_init_neon(void)
{
    // Сброс адресных линий
    TUBE_NEON_SEL_RESET();
    // Таймер ШИМ
    tube_init_timer(TIM2, RCC_APB1RSTR_TIM2RST, RCC_APB1ENR_TIM2EN);
    // Настройка канала 3
    TIM2->CCMR2 = TIM_CCMR2_OC3M_0 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;       // CC3 output, CC3 Fast off, CC3 preload off, CC3 mode PWM 2 (111)
    TIM2->CCER = TIM_CCER_CC3E;                                                 // CC3 output enable, CC3 Polarity high
}

void tube_init(void)
{
    // Инициализация буфера отображения
    MEMORY_CLEAR(tube_ram);
    // Подготовка таблицы гамма коррекции. TODO: подобрано субъективно, возможно есть смысл перенести гамму в настройку
    hmi_gamma_prepare(tube_gamma);
    // Инициализация ламп
    tube_init_nixie();
    tube_init_neon();
    // Обновление состояния ламп
    tube_refresh();
    // Выставление задачи мультиплексирования
    //event_timer_start_hz(DELEGATE_PROC(tube_mux), TUBE_MUX_HZ, EVENT_TIMER_PRI_CRITICAL | EVENT_TIMER_FLAG_LOOP); TODO:
}

OPTIMIZE_SPEED
void tube_refresh(void)
{
    hmi_rank_t i;
    hmi_sat_t sat;
    // Обновление цифр
    for (i = 0; i < TUBE_NIXIE_COUNT; i++)
    {
        if (tube_ram.nixie.data[i].unchanged)
            continue;
        tube_ram.nixie.data[i].unchanged = true;
        // Формирование цифры
        tube_ram.nixie.raw[i].digit = TUBE_NIXIE_SELC_CALC(TUBE_NIXIE_PIN_FIX[tube_ram.nixie.data[i].digit], 
                                                           tube_ram.nixie.data[i].dot);
        // Формирование насыщенности
        if (tube_ram.nixie.data[i].digit != TUBE_NIXIE_DIGIT_SPACE)
            sat = hmi_gamma_apply(tube_gamma, tube_ram.nixie.data[i].sat);
        else
            sat = 0;
        tube_ram.nixie.raw[i].sat = HMI_SAT_MAX - sat;
    }
    // Обновление неонок
    for (i = 0; i < TUBE_NEON_COUNT; i++)
    {
        if (tube_ram.neon.data[i].unchanged)
            continue;
        tube_ram.neon.data[i].unchanged = true;
        // Формирование насыщенности
        sat = hmi_gamma_apply(tube_gamma, tube_ram.neon.data[i].sat);
        tube_ram.neon.raw[i].sat = HMI_SAT_MAX - sat;
    }
}

void tube_nixie_digit_set(hmi_rank_t index, uint8_t digit, bool dot)
{
    // Проверка аргументов
    assert(digit <= TUBE_NIXIE_DIGIT_SPACE);
    assert(index < TUBE_NIXIE_COUNT);
    // Копирование значения
    tube_ram.nixie.data[index].dot = dot;
    tube_ram.nixie.data[index].digit = digit;
    tube_ram.nixie.data[index].unchanged = false;
}

void tube_nixie_sat_set(hmi_rank_t index, hmi_sat_t value)
{
    // Проверка аргументов
    assert(index < TUBE_NIXIE_COUNT);
    // Копирование значения
    tube_ram.nixie.data[index].sat = value;
    tube_ram.nixie.data[index].unchanged = false;
}

void tube_nixie_sat_set(hmi_sat_t value)
{
    for (hmi_rank_t i = 0; i < TUBE_NIXIE_COUNT; i++)
        tube_nixie_sat_set(i, value);
}

OPTIMIZE_SIZE
void tube_nixie_text_set(const char text[])
{
    uint8_t filled, length;
    // Проверка аргументов
    assert(strlen(text) <= UINT8_MAX);
    // Подсчет количества символов и точек
    filled = 0;
    length = (uint8_t)strlen(text);
    for (uint8_t i = 0; i < length; i++)
        switch (text[i])
        {
            case HMI_CHAR_DOT:
                break;
            default:
                assert(text[i] >= HMI_CHAR_ZERO && text[i] <= HMI_CHAR_NINE);
            case HMI_CHAR_SPACE:
                filled++;
                break;
        }
    const char *t = text;
    for (hmi_rank_t i = 0; i < TUBE_NIXIE_COUNT; i++)
    {
        bool dot = false;
        uint8_t digit = TUBE_NIXIE_DIGIT_SPACE;
        do
        {
            // Свободные места вначале
            if (filled < TUBE_NIXIE_COUNT)
            {
                filled++;
                continue;
            }
            // Пропуск точек
            for (; *t == HMI_CHAR_DOT; t++)
                dot = true;
            // Пробел
            if (*t == HMI_CHAR_SPACE)
            {
                t++;
                continue;
            }
            // Цифра
            digit = *t - HMI_CHAR_ZERO;
            t++;
        } while (false);
        tube_nixie_digit_set(i, digit, dot);
    }
}

void tube_neon_sat_set(hmi_rank_t index, hmi_sat_t value)
{
    // Проверка аргументов
    assert(index < TUBE_NEON_COUNT);
    // Копирование значения
    tube_ram.neon.data[index].sat = value;
    tube_ram.neon.data[index].unchanged = false;
}

void tube_neon_sat_set(hmi_sat_t value)
{
    for (hmi_rank_t i = 0; i < TUBE_NEON_COUNT; i++)
        tube_neon_sat_set(i, value);
}

// TODO: возможно выпилить
IRQ_ROUTINE
void tube_interrupt_nixie_selcrst(void)
{
    // Форсирование сброса катодного напряжения
    TUBE_NIXIE_SELC_SET(TUBE_NIXIE_SELC_CALC(TUBE_NIXIE_DIGIT_SPACE, false));
    TIM4->SR &= ~TIM_SR_UIF;                                                    // Clear UEV pending flag
}
