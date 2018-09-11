#include "tool.h"

// ����� UART ��� ����������� ������
#define UART_NO			0
// �������� ������ ��� ����������� ������
#define UART_BAUD		115200

// ��������������� ���������� �������� ������� �������������
E_SYMBOL void main_init(void);
// �� ����������� ������
C_SYMBOL void uart_div_modify(uint8 uart, uint32 divider);

// ��� ������ ��� �����������
#define MODULE_NAME     "ENTRY"

// ������ ��� ����� � ����������
static ROM void entry_wrap_task(void *dummy)
{
    // �������� ���������� TX
    while (READ_PERI_REG(UART_STATUS(UART_NO)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S))
    { }
    // �������� ������
    uart_div_modify(UART_NO, UART_CLK_FREQ / UART_BAUD);
    // ������ ����� (�������� ���������)
    log_raw("\033[2J");
    // �����������
    log_module(MODULE_NAME, "NixieClock communication frontend");
    log_module(MODULE_NAME, "Espressif SDK version: %s", system_get_sdk_version());
    // ID ���� �����������
    log_module(MODULE_NAME, "Core ID: %d", system_get_chip_id());
    // ID ���� �����
    log_module(MODULE_NAME, "Flash ID: %d", spi_flash_get_id());
	// ����� ���������� � ������
    log_heap(MODULE_NAME);
	system_print_meminfo();
	// ����� �������� ������� �������������
	main_init();
	// ����� �� ������
	vTaskDelete(NULL);
}

// ����� �����
C_SYMBOL ROM void user_init(void)
{
    // ������ ��������������� � ����� �������
    if (wifi_station_get_auto_connect())
        wifi_station_set_auto_connect(false);
    // ���������� ��������������� ����� ���������� �� ����� �������
    if (!wifi_station_get_reconnect_policy())
        wifi_station_set_reconnect_policy(true);
    // ���������� WiFi
    wifi_set_opmode(NULL_MODE);
    // ������ ������ �� �������������
	CREATE_TASK(entry_wrap_task, "entry", NULL);
}

#ifndef NDEBUG
// ����� Assert'��
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
