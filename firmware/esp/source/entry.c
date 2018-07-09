#include "typedefs.h"

// Номер UART для отладочного вывода
#define UART_NO			0
// Скорость обмена для отладочного вывода
#define UART_BAUD		115200

// Предварительное объявление основной функции инициализации
extern void main_init(void);

// Инициализация UART
static void ICACHE_FLASH_ATTR entry_uart_init(void)
{
	// Ожидание завершения TX
    while (READ_PERI_REG(UART_STATUS(UART_NO)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S))
    { }
	// Скорость обмена
    uart_div_modify(UART_NO, UART_CLK_FREQ / UART_BAUD);
    // Первый вывод (отчистка терминала)
    printf("\033[2J");
}

// Задача для входа в приложение
static void ICACHE_FLASH_ATTR entry_wrap_task(void *dummy)
{
	// Инициализация UART для логирования
	entry_uart_init();
	printf("NixieClock communication frontend\nEspressif SDK version: %s\n", system_get_sdk_version());
	// Вызов основной функции инициализации
	main_init();
	// Выход из задачи
	vTaskDelete(NULL);
}

// Точка входа
void ICACHE_FLASH_ATTR user_init(void)
{
	CREATE_TASK(entry_wrap_task, "entry", NULL);
}

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
uint32_t ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
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
