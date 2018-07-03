#include "io.h"

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


// Используемые GPIO
static gpio_t main_led_yellow, main_led_green;

// Тестовое задание
static void test_task(void *dummy)
{
	bool state;
    printf("Hello word!\n");
    for (state = false;;)
    {
    	state = !state;
    	gpio_state_set(&main_led_yellow, state);
    	vTaskDelay(500);
    }
}

// Точка входа
void ICACHE_FLASH_ATTR user_init(void)
{
    UART_WaitTxFifoEmpty(UART0);
    UART_WaitTxFifoEmpty(UART1);

    UART_ConfigTypeDef uart_config;
    uart_config.baud_rate    = BIT_RATE_115200;
    uart_config.data_bits     = UART_WordLength_8b;
    uart_config.parity          = USART_Parity_None;
    uart_config.stop_bits     = USART_StopBits_1;
    uart_config.flow_ctrl      = USART_HardwareFlowControl_None;
    uart_config.UART_RxFlowThresh = 120;
    uart_config.UART_InverseMask = UART_None_Inverse;
    UART_ParamConfig(UART0, &uart_config);

    UART_IntrConfTypeDef uart_intr;
    uart_intr.UART_IntrEnMask = UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA;
    uart_intr.UART_RX_FifoFullIntrThresh = 10;
    uart_intr.UART_RX_TimeOutIntrThresh = 2;
    uart_intr.UART_TX_FifoEmptyIntrThresh = 20;
    UART_IntrConfig(UART0, &uart_intr);

    UART_SetPrintPort(UART0);
    //UART_intr_handler_register(uart0_rx_intr_handler, NULL);
    //ETS_UART_INTR_ENABLE();


	printf("SDK version:%s\n", system_get_sdk_version());

	// Инициализация GPIO
	gpio_setup(&main_led_yellow,	4, PERIPHS_IO_MUX_GPIO4_U, 	FUNC_GPIO4, false);
	gpio_setup(&main_led_green,		5, PERIPHS_IO_MUX_GPIO5_U, 	FUNC_GPIO5, false);

	gpio_state_set(&main_led_yellow, true);
	gpio_state_set(&main_led_green, true);

    xTaskCreate(test_task, "green", 256, NULL, 2, NULL);
}
