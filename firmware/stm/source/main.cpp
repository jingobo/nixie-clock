#include "io.h"
#include "mcu.h"
#include "wdt.h"
#include "rtc.h"
#include "led.h"
#include "esp.h"
#include "nvic.h"
#include "wifi.h"
#include "temp.h"
#include "neon.h"
#include "ntime.h"
#include "nixie.h"
#include "event.h"
#include "light.h"
#include "timer.h"
#include "screen.h"
#include "display.h"
#include "storage.h"

// Точка входа в приложение
__task __noreturn void main(void)
{
    IRQ_CTX_DISABLE();
        // Системные модули (порядок не менять)
        nvic_init();
        mcu_init();
        wdt_init();
        rtc_init();
        io_init();
        timer_init();
        // Остальные модули
        storage_init();
        esp_init();
        led_init();
        neon_init();
        temp_init();
        light_init();
        nixie_init();
        screen_init();
        display_init();
        // Сервисы
        wifi_init();
        ntime_init();
    IRQ_CTX_ENABLE();

    // Обработка событий
    event_t::loop();
}
