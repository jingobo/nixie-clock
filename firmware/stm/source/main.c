#include "io.h"
#include "mcu.h"
#include "wdt.h"
#include "clk.h"
#include "rtc.h"
#include "led.h"
#include "esp.h"
#include "tube.h"
#include "nvic.h"
#include "event.h"
#include "display.h"

// Инициализация модулей
static void main_init(void)
{
    // Системные модули
    wdt_init();
    mcu_init();
    io_init();
    nvic_init();
    clk_init();
    rtc_init();
    event_init();
    // Остальные модули
    esp_init();
    led_init();
    tube_init();
    display_init();
}

uint16_t offset16 = 0;

    uint8_t last_digit = 0, cur_digit = 1, offset = 0;

bool led_enable = true;

static void display_update(void)
{
    if (!led_enable)
        return;
    for (hmi_rank_t i = 0; i < LED_COUNT; i++)
        led.data_set(i, hmi_hsv_t(((uint16_t)(i + offset) * 8) % 256, HMI_SAT_MAX, HMI_SAT_MAX).to_rgb());
    led.refresh();
    if (offset >= 31)
        offset = 0;
    else
        offset++;
    mcu_debug_pulse();
}



static void neon_sat_test(void)
{
    if (cur_digit > 9)
        cur_digit = 0;
    
    offset16++;
    if (offset16 <= 31)
    {
        if (offset16 & 1)
        {
            tube_nixie_digit_set(1, cur_digit, false);
            tube_nixie_sat_set(1, offset16 * 8);
        }
        else
        {
            tube_nixie_digit_set(1, last_digit, false);
            tube_nixie_sat_set(1, 255 - offset16 * 8);
        }
        tube_refresh();
    }
    if (offset16 == 32)
    {
        tube_nixie_sat_set(1, 186);
        tube_refresh();
    }
    if (offset16 >= 100)
    {
        offset16 = 0;
        last_digit = cur_digit;
        cur_digit++;
        if (cur_digit >= 10)
            cur_digit = 0;
    }
    //tube_nixie_sat_set(offset);
    //tube_neon_sat_set(offset);
    //tube_refresh();
}

static void led_enable_switch(void)
{
    led_enable = !led_enable;
    if (!led_enable)
    {
        for (hmi_rank_t i = 0; i < LED_COUNT; i++)
            led.data_set(i, hmi_rgb_t());
        led.refresh();
        
    }
}

// Для тестов
static void main_tests(void)
{
    led_filter_gamma.gamma_set(0.4f);
    led.attach(led_filter_gamma);
    
    tube_nixie_sat_set(255);
    tube_nixie_text_set("128900");
    tube_neon_sat_set(70);
    tube_refresh();
    event_timer_start_hz(display_update, 10, EVENT_TIMER_FLAG_LOOP);
    event_timer_start_hz(neon_sat_test, 100, EVENT_TIMER_FLAG_LOOP);

    event_timer_start_us(led_enable_switch, 10000000, EVENT_TIMER_FLAG_LOOP);

    event_timer_start_hz(esp_transaction, 1, EVENT_TIMER_FLAG_LOOP);

    
    
    // Проверка вывода частоты
    //RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;                                         // PA clock enable
    
    //util_reg_update_32(&GPIOA->CRH, /*GPIO_CRH_CNF8_1*/0, GPIO_CRH_CNF8);       // /Alt push pull
        
    //GPIOA->CRH |= GPIO_CRH_MODE8_0 | GPIO_CRH_MODE8_1;                          // Output 50 MHz
    
    /*bool reset = false;
    
    while (true)
    {
        clk_delay(6);
        if (reset)
            GPIOA->BSRR |= GPIO_BSRR_BS8;
        else
            GPIOA->BSRR |= GPIO_BSRR_BR8;
        reset = !reset;
    }
    
    clk_mco_output(CLK_MCO_SOURCE_SYS);*/
    
    // Проверка частоты RTC
    /*RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;                                         // PС clock enable
    util_reg_update_32(&GPIOC->CRH, GPIO_CRH_CNF13_1, GPIO_CRH_CNF13);          // Alt push pull
        
    GPIOC->CRH |= GPIO_CRH_MODE13_0 | GPIO_CRH_MODE13_1;                        // Output 50 MHz
    
    rtc_clock_output(true);*/    
}

static void led_sat_test(void)
{
    hmi_sat_t sat = 0;
    while (true)
    {
        clk_delay(10);
        //led_color_set(0, hmi_rgb_t(sat, 0, 0), true);
        if (sat >= HMI_SAT_MAX)
            sat = 0;
        else
            sat++;
        
        wdt_pulse();
    }
}

// Точка входа в приложение
__task __noreturn void main()
{
    main_init();
    // Выставление задач
    
    main_tests();
    
    // Главный цикл
    for (;;)
    {
        // Обработка событий
        for (bool again = true; again; again = event_execute())
        { }
        // Ожидание прерывания
        WFI();
    }
    // WTF?
}
