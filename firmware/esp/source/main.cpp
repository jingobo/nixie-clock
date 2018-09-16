#include "io.h"
#include "core.h"
#include "wifi.h"
#include "ntime.h"

ROM void main_init(void)
{
    // Инициализация модулей
    io_init();
    core_init();
    wifi_init();
    ntime_init();
}
