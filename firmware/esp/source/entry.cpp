#include "tool.h"

// Номер UART для отладочного вывода
#define UART_NO			0
// Скорость обмена для отладочного вывода
#define UART_BAUD		115200

// Предварительное объявление основной функции инициализации
E_SYMBOL void main_init(void);
// Не объявленный символ
C_SYMBOL void uart_div_modify(uint8 uart, uint32 divider);

// Имя модуля для логирования
#define MODULE_NAME     "ENTRY"

// Задача для входа в приложение
static ROM void entry_wrap_task(void *dummy)
{
    // Ожидание завершения TX
    while (READ_PERI_REG(UART_STATUS(UART_NO)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S))
    { }
    // Скорость обмена
    uart_div_modify(UART_NO, UART_CLK_FREQ / UART_BAUD);
    // Первый вывод (отчистка терминала)
    log_raw("\033[2J");
    // Приветствие
    log_module(MODULE_NAME, "NixieClock communication frontend");
    log_module(MODULE_NAME, "Espressif SDK version: %s", system_get_sdk_version());
    // ID чипа контроллера
    log_module(MODULE_NAME, "Core ID: %d", system_get_chip_id());
    // ID чипа флеша
    log_module(MODULE_NAME, "Flash ID: %d", spi_flash_get_id());
	// Вывод информации о памяти
    log_heap(MODULE_NAME);
	system_print_meminfo();
	// Вызов основной функции инициализации
	main_init();
	// Выход из задачи
	vTaskDelete(NULL);
}

// Точка входа
C_SYMBOL ROM void user_init(void)
{
    // Запрет вывода частоты кварца (я в шоке от китайцев)
    GPIO_OUT_SET(0, false);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    // Запрет автоподключения в точке доступа
    if (wifi_station_get_auto_connect())
        wifi_station_set_auto_connect(false);
    // Разрешение переподключения после отключения от точки доступа
    if (!wifi_station_get_reconnect_policy())
        wifi_station_set_reconnect_policy(true);
    // Отключение WiFi
    wifi_set_opmode(NULL_MODE);
    // Запуск задачи на инициализацию
	CREATE_TASK(entry_wrap_task, "entry", NULL);
}

#ifndef NDEBUG
// Отлов Assert'ов
C_SYMBOL ROM NO_RETURN void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    UNUSED(func);
    log_module(MODULE_NAME, "Assertion '%s' failed, at file %s:%d", expr, file, line);
    for (;;)
    { }
}
#endif

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
C_SYMBOL ROM uint32_t user_rf_cal_sector_set(void)
{
    switch (system_get_flash_size_map())
    {
        case FLASH_SIZE_4M_MAP_256_256:
            return 128 - 5;
        case FLASH_SIZE_8M_MAP_512_512:
        	return 256 - 5;
        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
        	return 512 - 5;
        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
        	return 1024 - 5;
    }
    return 0;
}
