#ifndef __IO_H
#define __IO_H

#include "system.h"

// --- Режимы порта IO --- //

#define IO_MODE_DECLARE(m, c)       (MASK_32(m, 0) | MASK_32(c, 2))
// Вход
#define IO_MODE_INPUT_ANALOG        IO_MODE_DECLARE(0, 0)
#define IO_MODE_INPUT_FLOATING      IO_MODE_DECLARE(0, 1)
#define IO_MODE_INPUT_PULL          IO_MODE_DECLARE(0, 2)
// Выход на 10 МГц
#define IO_MODE_OUTPUT_PP_10MHZ     IO_MODE_DECLARE(1, 0)
#define IO_MODE_OUTPUT_OD_10MHZ     IO_MODE_DECLARE(1, 1)
#define IO_MODE_OUTPUT_APP_10MHZ    IO_MODE_DECLARE(1, 2)
#define IO_MODE_OUTPUT_AOD_10MHZ    IO_MODE_DECLARE(1, 3)
// Выход на 2 МГц
#define IO_MODE_OUTPUT_PP_2MHZ      IO_MODE_DECLARE(2, 0)
#define IO_MODE_OUTPUT_OD_2MHZ      IO_MODE_DECLARE(2, 1)
#define IO_MODE_OUTPUT_APP_2MHZ     IO_MODE_DECLARE(2, 2)
#define IO_MODE_OUTPUT_AOD_2MHZ     IO_MODE_DECLARE(2, 3)
// Выход на 50 МГц
#define IO_MODE_OUTPUT_PP_50MHZ     IO_MODE_DECLARE(3, 0)
#define IO_MODE_OUTPUT_OD_50MHZ     IO_MODE_DECLARE(3, 1)
#define IO_MODE_OUTPUT_APP_50MHZ    IO_MODE_DECLARE(3, 2)
#define IO_MODE_OUTPUT_AOD_50MHZ    IO_MODE_DECLARE(3, 3)
// Выход по умолчанию
#define IO_MODE_OUTPUT_PP           IO_MODE_OUTPUT_PP_10MHZ
#define IO_MODE_OUTPUT_OD           IO_MODE_OUTPUT_OD_10MHZ
#define IO_MODE_OUTPUT_APP          IO_MODE_OUTPUT_APP_10MHZ
#define IO_MODE_OUTPUT_AOD          IO_MODE_OUTPUT_AOD_10MHZ

// Конфигурирование режима порта (расширенное)
#define IO_PORT_CFG_EXT(n, mode, crl, crh)              \
    SAFE_BLOCK(                                         \
        (((n) > 7) ?                                    \
            ((crh) &= ~MASK_32(0x0F, ((n) - 8) * 4)) :  \
            ((crl) &= ~MASK_32(0x0F, (n) * 4)));        \
        (((n) > 7) ?                                    \
            ((crh) |= MASK_32(mode, ((n) - 8) * 4)) :   \
            ((crl) |= MASK_32(mode, (n) * 4)))          \
    )
// Конфигурирование режима порта (по выводу)
#define IO_PORT_CFG(out, mode)          IO_PORT_CFG_EXT(out, mode, (out##_PORT)->CRL, (out##_PORT)->CRH)

// --- Установка/Сброс вывода порта --- //

// Получает маску порта по индексу
#define IO_MASK(index)                  MASK_32(1, index)
        
// Установка порта в LOW (по маске)
#define IO_PORT_RESET_MASK(port, mask)  (port)->BSRR |= MASK_32(mask, 16)
// Установка порта в LOW (по выводу)
#define IO_PORT_RESET(out)              IO_PORT_RESET_MASK(out##_PORT, IO_MASK(out))

// Установка порта в HIGH (по маске)
#define IO_PORT_SET_MASK(port, mask)    (port)->BSRR |= MASK_32(mask, 0)
// Установка порта в HIGH (по выводу)
#define IO_PORT_SET(out)                IO_PORT_SET_MASK(out##_PORT, IO_MASK(out))
        
// --- Порт A --- //

#define IO_RSV0                         0
#define IO_RSV0_PORT                    GPIOA

#define IO_RSV1                         1
#define IO_RSV1_PORT                    GPIOA

#define IO_DBG_TX                       2
#define IO_DBG_TX_PORT                  GPIOA

#define IO_DBG_RX                       3
#define IO_DBG_RX_PORT                  GPIOA

// Зарезервирован под отладочный пульс
#define IO_RSV4                         4
#define IO_RSV4_PORT                    GPIOA

#define IO_TSELA0                       5
#define IO_TSELA0_PORT                  GPIOA

#define IO_TSELA1                       6
#define IO_TSELA1_PORT                  GPIOA

#define IO_TSELA2                       7
#define IO_TSELA2_PORT                  GPIOA

#define IO_SSEL0                        8
#define IO_SSEL0_PORT                   GPIOA

#define IO_SSEL1                        9
#define IO_SSEL1_PORT                   GPIOA

#define IO_LED                          10
#define IO_LED_PORT                     GPIOA

#define IO_BTN0                         11
#define IO_BTN0_PORT                    GPIOA

#define IO_BTN1                         12
#define IO_BTN1_PORT                    GPIOA

#define IO_SWDIO                        13
#define IO_SWDIO_PORT                   GPIOA

#define IO_SWCLK                        14
#define IO_SWCLK_PORT                   GPIOA
        
#define IO_ESP_CS                       15
#define IO_ESP_CS_PORT                  GPIOA

// --- Порт B --- //

#define IO_RSV5                         0
#define IO_RSV5_PORT                    GPIOB

#define IO_RSV6                         1
#define IO_RSV6_PORT                    GPIOB

#define IO_BOOT1                        2
#define IO_BOOT1_PORT                   GPIOB

#define IO_ESP_CLK                      3
#define IO_ESP_CLK_PORT                 GPIOB

#define IO_ESP_MISO                     4
#define IO_ESP_MISO_PORT                GPIOB

#define IO_ESP_MOSI                     5
#define IO_ESP_MOSI_PORT                GPIOB

#define IO_DS_WIRE                      6
#define IO_DS_WIRE_PORT                 GPIOB

#define IO_TPWM                         7
#define IO_TPWM_PORT                    GPIOB

#define IO_LS_SCL                       8
#define IO_LS_SCL_PORT                  GPIOB

#define IO_LS_SDA                       9
#define IO_LS_SDA_PORT                  GPIOB

#define IO_SPWM                         10
#define IO_SPWM_PORT                    GPIOB

#define IO_TSELP                        11
#define IO_TSELP_PORT                   GPIOB

#define IO_TSELC0                       12
#define IO_TSELC0_PORT                  GPIOB

#define IO_TSELC1                       13
#define IO_TSELC1_PORT                  GPIOB

#define IO_TSELC2                       14
#define IO_TSELC2_PORT                  GPIOB

#define IO_TSELC3                       15
#define IO_TSELC3_PORT                  GPIOB

// --- Порт C --- //

#define IO_ESP_RST                      13
#define IO_ESP_RST_PORT                 GPIOC

#define IO_OSC32_IN                     14
#define IO_OSC32_IN_PORT                GPIOC

#define IO_OSC32_OUT                    15
#define IO_OSC32_OUT_PORT               GPIOC

// --- Порт D --- //

#define IO_OSC_IN                       0
#define IO_OSC_IN_PORT                  GPIOD
        
#define IO_OSC_OUT                      1
#define IO_OSC_OUT_PORT                 GPIOD

// --- Вывод тактовой частоты ---- //

#define IO_MCO                          IO_SSEL0
#define IO_MCO_PORT                     IO_SSEL0_PORT
#define IO_MCO_OUTPUT_MODE              IO_MODE_OUTPUT_PP

// Инициализация модуля
void io_init(void);

#endif // __IO_H
