#include "io.h"
#include "timer.h"
#include "nixie.h"
#include "screen.h"
#include "system.h"

// Смещение насыщенности для сброса PWM после UEV
constexpr const hmi_sat_t NIXIE_SAT_DX = 1;
// Частота мультиплексирования ламп [Гц]
constexpr const uint32_t NIXIE_MUX_HZ = HMI_FRAME_RATE * NIXIE_COUNT;

// Сброс адресной линии анодов
static void nixie_sela_reset(void)
{
    IO_PORT_RESET_MASK(IO_TSELA0_PORT,
        IO_MASK(IO_TSELA0) |
        IO_MASK(IO_TSELA1) |
        IO_MASK(IO_TSELA2));
}

// Сброс адресной линии катодов
static void nixie_selc_reset(void)
{
    IO_PORT_RESET_MASK(IO_TSELP_PORT,
        IO_MASK(IO_TSELP) |
        IO_MASK(IO_TSELC0) |
        IO_MASK(IO_TSELC1) |
        IO_MASK(IO_TSELC2) |
        IO_MASK(IO_TSELC3));
}

// Подсчет значения адресной линии катодов
static constexpr uint8_t nixie_selc_calc(uint8_t index, bool dot)
{
    return MASK(uint8_t, index, 1) | (dot ? 1 : 0);
}

// Событие одновления дисплея
static event_callback_t nixie_screen_refresh([](void)
{
    screen.refresh();
});

// Драйвер вывода ламп
static class nixie_display_t : public nixie_model_t::display_t
{
    // Данные лампы (прерывание)
    struct irq_t
    {
        // Ширина импульса
        uint8_t pw;
        // Адрес катода
        uint8_t selc;
        
        // Конструктор по умолчанию
        irq_t(void) : pw(0), selc(nixie_selc_calc(NIXIE_DIGIT_SPACE, false))
        { }
        
        // Конструктор с данными
        irq_t(uint8_t _pw, uint8_t _selc) : pw(_pw), selc(_selc)
        { }
    } irq[NIXIE_COUNT];

    // Данные лампы (вывод)
    struct out_t
    {
        // Для определения обновления
        bool unchanged = false;
    } out[NIXIE_COUNT];
    
    // Индекс выводимой дампы (мультиплексирование)
    uint8_t nmi = 0;

    // Таблица перевода реальной цифры в цифру полученную после трассировки
    static constexpr const uint8_t PIN_FIX[11] =
    {
        8, 3, 1, 6, 9, 0, 4, 2, 7, 5, 10
    };
protected:
    // Обработчик изменения данных
    virtual void data_changed(hmi_rank_t index, nixie_data_t &data) override final
    {
        // Установка данных
        out[index].unchanged = false;
    }
    
    // Обновление состояния ламп
    virtual void refresh(void) override final
    {
        // Базовый метод
        display_t::refresh();
        
        // Пересчет данных прерываний
        for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
        {
            if (out[i].unchanged)
                continue;
            out[i].unchanged = true;
            
            // Получаем данные
            const auto data = in_get(i);
            // Проверка цифры
            assert(data.digit <= NIXIE_DIGIT_SPACE);
            
            // Формирование ширины импульса по насыщенности
            const hmi_sat_t sat = (data.digit != NIXIE_DIGIT_SPACE) ?
                data.sat :
                HMI_SAT_MIN;
            const auto pw = HMI_SAT_MAX - HMI_GAMMA_TABLE[sat];
                
            // Формирование цифры
            const auto selc = nixie_selc_calc(PIN_FIX[data.digit], data.dot);
            
            // Конечные данные к прерыванию
            const irq_t irq_data(pw, selc);
            
            // Перенос данных с запретом прерываний
            IRQ_SAFE_ENTER();
                irq[i] = irq_data;
            IRQ_SAFE_LEAVE();
        }
    }
public:
    // Мультиплексирование
    void mux(void)
    {
        // Переключение катодного напряжения
        nixie_selc_reset();
        IO_PORT_SET_MASK(IO_TSELC0_PORT, MASK_32(irq[nmi].selc, IO_TSELP));
        
        // Переключение анодного напряжения
        nixie_sela_reset();
        IO_PORT_SET_MASK(IO_TSELA0_PORT, MASK_32(nmi, IO_TSELA0));
        
        // PWM
        TIM4->CNT = (irq[nmi].pw >> 1) + NIXIE_SAT_DX;                          // Update counter (center aligned)
        TIM4->CCR2 = irq[nmi].pw + NIXIE_SAT_DX;                                // Update CC2 value
        TIM4->CR1 |= TIM_CR1_CEN;                                               // TIM enable

        // Переход к следующей лампе
        if (++nmi < NIXIE_COUNT)
            return;
        nmi = 0;
        
        // Обновление происходит здесь для улучшеной синхронизации
        nixie_screen_refresh.raise();
    }
} nixie_display;

// Таймер мультиплексирования ламп
static timer_callback_t nixie_mux_timer([](void)
{
    nixie_display.mux();
});

void nixie_init(void)
{
    // Сброс адресных линий
    nixie_sela_reset();
    nixie_selc_reset();
    
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
    screen.nixie.attach(nixie_display);
    // Выставление задачи мультиплексирования
    nixie_mux_timer.start_hz(NIXIE_MUX_HZ, TIMER_PRI_CRITICAL | TIMER_FLAG_LOOP);
}

// Развязочная таблица, указаны цифры по убыванию в глубь лампы
constexpr const uint8_t NIXIE_DIGITS_BY_PLACES[10] = 
{ 
    3, 8, 9, 4, 0, 5, 7, 2, 6, 1,
};

// Развязочная таблица, указаны места по возрастанию цифры
constexpr const uint8_t NIXIE_PLACES_BY_DIGITS[10] = 
{
    4, 9, 7, 0, 3, 5, 8, 6, 1, 2,
};

// Получает следующиую цифру по месту "из глубины"
static uint8_t nixie_next_digit_out_depth(uint8_t digit)
{
    auto place = NIXIE_PLACES_BY_DIGITS[digit];
    if (place > 0)
        place--;
    else
        place = 9;
    return NIXIE_DIGITS_BY_PLACES[place];
}

// Получает следующиую цифру по месту "в глубину"
static uint8_t nixie_next_digit_in_depth(uint8_t digit)
{
    auto place = NIXIE_PLACES_BY_DIGITS[digit];
    if (place < 9)
        place++;
    else
        place = 0;
    return NIXIE_DIGITS_BY_PLACES[place];
}

void nixie_switcher_t::refresh_smooth_default(nixie_data_t &data, effect_data_t &effect)
{
    // Количество фреймов одной цифры
    constexpr const auto FRAME_COUNT = HMI_SMOOTH_FRAME_COUNT / 2;
    
    // Фреймы исчезновения старой цифры
    if (effect.frame < FRAME_COUNT)
    {
        data.sat = math_value_ratio<hmi_sat_t>(data.sat, HMI_SAT_MIN, effect.frame, FRAME_COUNT);
        data.digit = effect.digit_from;
        return;
    }
    
    // Фреймы появления новой цифры
    if (effect.frame < HMI_SMOOTH_FRAME_COUNT)
    {
        data.sat = math_value_ratio<hmi_sat_t>(HMI_SAT_MIN, data.sat, effect.frame - FRAME_COUNT, FRAME_COUNT);
        return;
    }
    
    // Завершение эффекта
    effect.stop();
}

void nixie_switcher_t::refresh_smooth_substitution(nixie_data_t &data, effect_data_t &effect)
{
    // Количество фреймов одной цифры
    constexpr const auto FRAME_COUNT = HMI_SMOOTH_FRAME_COUNT * 2;
        
    // Фреймы смены цифры
    if (effect.frame < FRAME_COUNT)
    {
        const auto sat = math_value_ratio<hmi_sat_t>(HMI_SAT_MIN, HMI_SAT_MAX, effect.frame, FRAME_COUNT);
        if (effect.frame & 1)
            data.sat = sat;
        else
        {
            data.sat = HMI_SAT_MAX - sat;
            data.digit = effect.digit_from;
        }
        
        return;
    }
    
    // Фреймы довода насыщенности
    if (effect.frame < FRAME_COUNT * 2)
    {
        const auto sat = math_value_ratio<hmi_sat_t>(180, HMI_SAT_MAX, effect.frame - FRAME_COUNT, FRAME_COUNT);
        data.sat = math_value_ratio<hmi_sat_t>(HMI_SAT_MIN, sat, data.sat, HMI_SAT_MAX);
        return;
    }
    
    // Завершение эффекта
    effect.stop();
}

void nixie_switcher_t::refresh_switch(nixie_data_t &data, effect_data_t &effect)
{
    // Прескалер фреймов эффекта
    constexpr const auto FRAME_PRECALER = HMI_SMOOTH_FRAME_COUNT * 2 / NIXIE_DIGIT_SPACE;

    // Текущая изменяемая цифра
    auto &digit = effect.digit_from;
    
    // Прескалер смены
    if (effect.frame % FRAME_PRECALER != 0)
    {
        data.digit = digit;
        return;
    }
    
    // Признак приблежения к нормальной цифре
    const auto to_normal = effect.digit_to != NIXIE_DIGIT_SPACE;
    
    uint8_t place;
    switch (settings.effect)
    {
        case EFFECT_SWITCH_DEF:
            if (++digit >= NIXIE_DIGIT_SPACE && to_normal)
                digit = 0;
            break;
            
        case EFFECT_SWITCH_IN:
            place = NIXIE_PLACES_BY_DIGITS[digit];
            if (place != 9 || to_normal)
                digit = nixie_next_digit_in_depth(digit);
            else
                digit = NIXIE_DIGIT_SPACE;
            break;
            
        case EFFECT_SWITCH_OUT:
            place = NIXIE_PLACES_BY_DIGITS[digit];
            if (place != 0 || to_normal)
                digit = nixie_next_digit_out_depth(digit);
            else
                digit = NIXIE_DIGIT_SPACE;
            break;
            
        default:
            assert(false);
            break;
    }
    
    data.digit = digit;
}

void nixie_switcher_t::refresh(void)
{
    // Базовый метод
    nixie_filter_t::refresh();
    
    // Выход если эффект не выбран
    if (is_effect_empty())
        return;

    // Обход разрядов
    for (hmi_rank_t i = 0; i < NIXIE_COUNT; i++)
    {
        // Пропуск если эффект не запущен
        auto &effect = effects[i];
        if (effect.inactive())
            continue;
        
        // Входные данные
        auto data = in_get(i);
        
        // Выполнение эффекта
        switch (settings.effect)
        {
            case EFFECT_SMOOTH_DEF:
                refresh_smooth_default(data, effect);
                break;
                
            case EFFECT_SMOOTH_SUB:
                refresh_smooth_substitution(data, effect);
                break;
                
            case EFFECT_SWITCH_DEF:
            case EFFECT_SWITCH_IN:
            case EFFECT_SWITCH_OUT:
                refresh_switch(data, effect);
                break;
                
            default:
                assert(false);
                break;
        }
        
        // Передача данных
        effect.frame++;
        out_set(i, data);
    }
}

void nixie_switcher_t::data_changed(hmi_rank_t index, nixie_data_t &data)
{
    // Реакция только на изменение цифры
    auto last = in_get(index).digit;
    const auto current = data.digit;
    if (last == current || is_effect_empty())
    {
        // Базовый метод
        nixie_filter_t::data_changed(index, data);
        return;
    }
    
    // Старт эффекта
    switch (settings.effect)
    {
        case EFFECT_SMOOTH_DEF:
        case EFFECT_SMOOTH_SUB:
            // Ничего
            break;
            
        case EFFECT_SWITCH_DEF:
            // Переход к следующей цифре
            last = current + 1;
            if (last >= NIXIE_DIGIT_SPACE)
                last = 0;
            break;
            
        case EFFECT_SWITCH_IN:
            // Переход к следующему месту
            if (current >= NIXIE_DIGIT_SPACE)
                last = NIXIE_DIGITS_BY_PLACES[0];
            else
                last = nixie_next_digit_in_depth(current);
            break;
            
        case EFFECT_SWITCH_OUT:
            // Переход к следующему месту
            if (current >= NIXIE_DIGIT_SPACE)
                last = NIXIE_DIGITS_BY_PLACES[9];
            else
                last = nixie_next_digit_out_depth(current);
            break;
            
        default:
            assert(false);
            break;
    }
    
    effects[index].start(last, current);
}
