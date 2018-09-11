#include "io.h"
#include "tube.h"
#include "nvic.h"
#include "event.h"
#include "system.h"

// �������� ������������ ��� ����� PWM ����� UEV
#define TUBE_SAT_DX                     1
// ������� ������������������� ���� [��]
#define TUBE_MUX_HZ                     (HMI_FRAME_RATE * TUBE_NIXIE_COUNT)

// ����� �������� ����� ������
#define TUBE_NIXIE_SELA_RESET()         \
    IO_PORT_RESET_MASK(IO_TSELA0_PORT,  \
        IO_MASK(IO_TSELA0) |            \
        IO_MASK(IO_TSELA1) |            \
        IO_MASK(IO_TSELA2))

// ��������� �������� ����� ������
#define TUBE_NIXIE_SELA_SET(v)          \
    TUBE_NIXIE_SELA_RESET();            \
    IO_PORT_SET_MASK(IO_TSELA0_PORT, MASK_32(v, IO_TSELA0))

// ����� �������� ����� �������
#define TUBE_NIXIE_SEL�_RESET()         \
    IO_PORT_RESET_MASK(IO_TSELP_PORT,   \
        IO_MASK(IO_TSELP) |             \
        IO_MASK(IO_TSELC0) |            \
        IO_MASK(IO_TSELC1) |            \
        IO_MASK(IO_TSELC2) |            \
        IO_MASK(IO_TSELC3))

// ��������� �������� ����� �������
#define TUBE_NIXIE_SELC_SET(v)          \
    TUBE_NIXIE_SEL�_RESET();            \
    IO_PORT_SET_MASK(IO_TSELC0_PORT, MASK_32(v, IO_TSELP))

// ������� �������� �������� ����� �������
#define TUBE_NIXIE_SELC_CALC(v, dot)    \
    (MASK(uint8_t, v, 1) | (dot ? 1 : 0))
        
// ����� �������� ����� ������
#define TUBE_NEON_SEL_RESET()           \
    IO_PORT_RESET_MASK(IO_SSEL0_PORT,   \
        IO_MASK(IO_SSEL0) |             \
        IO_MASK(IO_SSEL1))

// ��������� �������� ����� ������
#define TUBE_NEON_SEL_SET(v)            \
    TUBE_NEON_SEL_RESET();              \
    IO_PORT_SET_MASK(IO_SSEL0_PORT, MASK_32(v, IO_SSEL0))
        
// ������� ��������� ������ ����
typedef struct
{
    // ������������
    hmi_sat_t sat;
} tube_data_t;

// ������ �������� ����� (����������)
typedef struct : tube_data_t
{
    // ������������ �����
    uint8_t digit;
} tube_nixie_irq_t;

// ������ �������� �����
typedef struct : tube_nixie_irq_t
{
    // ��������� �����
    bool dot : 1;
    // ��� ����������� ����������
    bool unchanged : 1;
} tube_nixie_t;

// ������ �������� ����� (����������)
typedef tube_data_t tube_neon_irq_t;

// ������ �������� ����� 
typedef struct : tube_neon_irq_t
{
    // ��� ����������� ����������
    bool unchanged : 1;
} tube_neon_t;

// ���������� ��� ��� ������
template <hmi_rank_t count, typename N, typename R>
class tube_ram_s
{
public:
    // ������ ��� �����������
    N data[count];
    // ������ ��� ����������� (����������)
    R raw[count];
    // ������ ��� �������������������
    hmi_rank_t mux;
    
    // ��������� �������������������
    INLINE_FORCED
    void mux_next(void)
    {
        if (++mux >= count)
            mux = 0;
    }
};

// ��� ������ ������
static __no_init struct
{
    // �������� �����
    tube_ram_s<TUBE_NIXIE_COUNT, tube_nixie_t, tube_nixie_irq_t> nixie;
    // �������� �����
    tube_ram_s<TUBE_NEON_COUNT, tube_neon_t, tube_neon_irq_t> neon;
} tube_ram;

// ������� ����� ��������
static __no_init hmi_sat_table_t tube_gamma;

// ������� �������� �������� ����� � ����� ���������� ����� �����������
static const uint8_t TUBE_NIXIE_PIN_FIX[11] =
{ 8, 3, 1, 6, 9, 0, 4, 2, 7, 5, 10 };

// ����������������� ������� ���
#define TUBE_PWM_SET(TIM, V, CH)                                                    \
    sat = (V);                                                                      \
    (TIM)->CNT = (sat >> 1) + TUBE_SAT_DX;  /* Update counter (center aligned) */   \
    (TIM)->CCR##CH = sat + TUBE_SAT_DX;     /* Update CCx value */                  \
    (TIM)->CR1 |= TIM_CR1_CEN               /* TIM enable */

// �������������������
OPTIMIZE_SPEED
static void tube_mux(void)
{
    hmi_sat_t sat;
    // ������������ ��������� ���������� (�����)
    TUBE_NIXIE_SELC_SET(tube_ram.nixie.raw[tube_ram.nixie.mux].digit);
    // ��������� �������� ���������� (�����)
    TUBE_NIXIE_SELA_SET(tube_ram.nixie.mux);
    // ��������� �������� ���������� (������)
    TUBE_NEON_SEL_SET(tube_ram.neon.mux);

    // PWM (�����)
    TUBE_PWM_SET(TIM4, tube_ram.nixie.raw[tube_ram.nixie.mux].sat, 2);
    // PWM (������)
    TUBE_PWM_SET(TIM2, tube_ram.neon.raw[tube_ram.neon.mux].sat, 3);

    // ������� � ��������� �����
    tube_ram.nixie.mux_next();
    tube_ram.neon.mux_next();
}

// ������� ������������� ����������� ������� ��� ���
static void tube_init_timer(TIM_TypeDef *TIM, uint32_t reset_flag, uint32_t clock_flag)
{
    // ������������ � ����� �������
    RCC->APB1ENR |= clock_flag;                                                 // TIM clock enable
    RCC->APB1RSTR |= clock_flag;                                                // TIM reset
    RCC->APB1RSTR &= ~clock_flag;                                               // TIM unreset
    // ������� ���������������� ������� ��� PWM
    TIM->CR1 = TIM_CR1_OPM;                                                     // TIM disable, UEV on, OPM on, Up, CMS edge, Clock /1, ARR preload off
    TIM->PSC = FMCU_NORMAL_HZ / (TUBE_MUX_HZ * (HMI_SAT_COUNT + 1)) - 1;        // Prescaler (frequency)
    TIM->ARR = HMI_SAT_MAX + TUBE_SAT_DX - 1;                                   // PWM Period
    TIM->BDTR = TIM_BDTR_MOE;                                                   // Main Output enable
}

// ������������� �������� ����
INLINE_FORCED
static void tube_init_nixie(void)
{
    // ����� �������� �����
    TUBE_NIXIE_SELA_RESET();
    TUBE_NIXIE_SEL�_RESET();
    // ������ ���
    tube_init_timer(TIM4, RCC_APB1RSTR_TIM4RST, RCC_APB1ENR_TIM4EN);
    // ��������� ������ 2
    TIM4->CCMR1 = TIM_CCMR1_OC2M_0 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;       // CC2 output, CC2 Fast off, CC2 preload off, CC2 mode PWM 2 (111)
    TIM4->CCER = TIM_CCER_CC2E;                                                 // CC2 output enable, CC2 Polarity high
    // TODO: ��������
    //TIM4->DIER = TIM_DIER_UIE;                                                  // UEV interrupt enable
    //nvic_irq_enable_set(TIM4_IRQn, true);
}

// ������������� �������� ����
INLINE_FORCED
static void tube_init_neon(void)
{
    // ����� �������� �����
    TUBE_NEON_SEL_RESET();
    // ������ ���
    tube_init_timer(TIM2, RCC_APB1RSTR_TIM2RST, RCC_APB1ENR_TIM2EN);
    // ��������� ������ 3
    TIM2->CCMR2 = TIM_CCMR2_OC3M_0 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;       // CC3 output, CC3 Fast off, CC3 preload off, CC3 mode PWM 2 (111)
    TIM2->CCER = TIM_CCER_CC3E;                                                 // CC3 output enable, CC3 Polarity high
}

void tube_init(void)
{
    // ������������� ������ �����������
    MEMORY_CLEAR(tube_ram);
    // ���������� ������� ����� ���������. TODO: ��������� �����������, �������� ���� ����� ��������� ����� � ���������
    hmi_gamma_prepare(tube_gamma);
    // ������������� ����
    tube_init_nixie();
    tube_init_neon();
    // ���������� ��������� ����
    tube_refresh();
    // ����������� ������ �������������������
    //event_timer_start_hz(DELEGATE_PROC(tube_mux), TUBE_MUX_HZ, EVENT_TIMER_PRI_CRITICAL | EVENT_TIMER_FLAG_LOOP); TODO:
}

OPTIMIZE_SPEED
void tube_refresh(void)
{
    hmi_rank_t i;
    hmi_sat_t sat;
    // ���������� ����
    for (i = 0; i < TUBE_NIXIE_COUNT; i++)
    {
        if (tube_ram.nixie.data[i].unchanged)
            continue;
        tube_ram.nixie.data[i].unchanged = true;
        // ������������ �����
        tube_ram.nixie.raw[i].digit = TUBE_NIXIE_SELC_CALC(TUBE_NIXIE_PIN_FIX[tube_ram.nixie.data[i].digit], 
                                                           tube_ram.nixie.data[i].dot);
        // ������������ ������������
        if (tube_ram.nixie.data[i].digit != TUBE_NIXIE_DIGIT_SPACE)
            sat = hmi_gamma_apply(tube_gamma, tube_ram.nixie.data[i].sat);
        else
            sat = 0;
        tube_ram.nixie.raw[i].sat = HMI_SAT_MAX - sat;
    }
    // ���������� ������
    for (i = 0; i < TUBE_NEON_COUNT; i++)
    {
        if (tube_ram.neon.data[i].unchanged)
            continue;
        tube_ram.neon.data[i].unchanged = true;
        // ������������ ������������
        sat = hmi_gamma_apply(tube_gamma, tube_ram.neon.data[i].sat);
        tube_ram.neon.raw[i].sat = HMI_SAT_MAX - sat;
    }
}

void tube_nixie_digit_set(hmi_rank_t index, uint8_t digit, bool dot)
{
    // �������� ����������
    assert(digit <= TUBE_NIXIE_DIGIT_SPACE);
    assert(index < TUBE_NIXIE_COUNT);
    // ����������� ��������
    tube_ram.nixie.data[index].dot = dot;
    tube_ram.nixie.data[index].digit = digit;
    tube_ram.nixie.data[index].unchanged = false;
}

void tube_nixie_sat_set(hmi_rank_t index, hmi_sat_t value)
{
    // �������� ����������
    assert(index < TUBE_NIXIE_COUNT);
    // ����������� ��������
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
    // �������� ����������
    assert(strlen(text) <= UINT8_MAX);
    // ������� ���������� �������� � �����
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
            // ��������� ����� �������
            if (filled < TUBE_NIXIE_COUNT)
            {
                filled++;
                continue;
            }
            // ������� �����
            for (; *t == HMI_CHAR_DOT; t++)
                dot = true;
            // ������
            if (*t == HMI_CHAR_SPACE)
            {
                t++;
                continue;
            }
            // �����
            digit = *t - HMI_CHAR_ZERO;
            t++;
        } while (false);
        tube_nixie_digit_set(i, digit, dot);
    }
}

void tube_neon_sat_set(hmi_rank_t index, hmi_sat_t value)
{
    // �������� ����������
    assert(index < TUBE_NEON_COUNT);
    // ����������� ��������
    tube_ram.neon.data[index].sat = value;
    tube_ram.neon.data[index].unchanged = false;
}

void tube_neon_sat_set(hmi_sat_t value)
{
    for (hmi_rank_t i = 0; i < TUBE_NEON_COUNT; i++)
        tube_neon_sat_set(i, value);
}

// TODO: �������� ��������
IRQ_ROUTINE
void tube_interrupt_nixie_selcrst(void)
{
    // ������������ ������ ��������� ����������
    TUBE_NIXIE_SELC_SET(TUBE_NIXIE_SELC_CALC(TUBE_NIXIE_DIGIT_SPACE, false));
    TIM4->SR &= ~TIM_SR_UIF;                                                    // Clear UEV pending flag
}
