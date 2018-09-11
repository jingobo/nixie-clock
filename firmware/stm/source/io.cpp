#include "io.h"

// Загрузка регистров в буферы
#define IO_LOAD(p)              \
    odr = 0;                    \
    crl = GPIO##p->CRL;         \
    crh = GPIO##p->CRH

// Возврат в регистры из буферов
#define IO_SAVE(p)              \
    GPIO##p->ODR = odr;         \
    GPIO##p->CRL = crl;         \
    GPIO##p->CRH = crh

// Инициализация вывода порта
#define IO_INIT(out, mode)      IO_PORT_CFG_EXT(out, mode, crl, crh)
// Установка состояния вывода
#define IO_HIGH(out)            odr |= IO_MASK(out)

OPTIMIZE_SIZE
void io_init(void)
{
    // Промежуточные буферы для формиорвания значений регистров
    uint32_t crl, crh, odr;
    // Конфигурирование IO по умолчанию
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN |                   // PA, PB clock enable
                    RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN;                    // PC, PD clock enable
    // Порт A
    IO_LOAD(A);
        IO_INIT(IO_RSV0,        IO_MODE_INPUT_PULL);
        IO_INIT(IO_RSV1,        IO_MODE_INPUT_PULL);
        IO_INIT(IO_DBG_TX,      IO_MODE_OUTPUT_APP);    IO_HIGH(IO_DBG_TX);
        IO_INIT(IO_DBG_RX,      IO_MODE_INPUT_PULL);    IO_HIGH(IO_DBG_RX);
        IO_INIT(IO_RSV4,        IO_MODE_OUTPUT_PP); // TODO: в IO_MODE_INPUT_PULL
        IO_INIT(IO_TSELA0,      IO_MODE_OUTPUT_PP);
        IO_INIT(IO_TSELA1,      IO_MODE_OUTPUT_PP);
        IO_INIT(IO_TSELA2,      IO_MODE_OUTPUT_PP);
        IO_INIT(IO_MCO,         IO_MCO_OUTPUT_MODE);
        IO_INIT(IO_SSEL1,       IO_MODE_OUTPUT_PP);
        IO_INIT(IO_LED,         IO_MODE_OUTPUT_APP);
        IO_INIT(IO_BTN0,        IO_MODE_INPUT_PULL);    IO_HIGH(IO_BTN0);
        IO_INIT(IO_BTN1,        IO_MODE_INPUT_PULL);    IO_HIGH(IO_BTN1);
        IO_INIT(IO_SWDIO,       IO_MODE_INPUT_PULL);
        IO_INIT(IO_SWCLK,       IO_MODE_INPUT_PULL);
        IO_INIT(IO_ESP_CS,      IO_MODE_OUTPUT_PP);     IO_HIGH(IO_ESP_CS);
    IO_SAVE(A);
    // Порт B
    IO_LOAD(B);
        IO_INIT(IO_RSV5,        IO_MODE_INPUT_PULL);
        IO_INIT(IO_RSV6,        IO_MODE_INPUT_PULL);
        IO_INIT(IO_BOOT1,       IO_MODE_INPUT_PULL);
        IO_INIT(IO_ESP_CLK,     IO_MODE_OUTPUT_APP);
        IO_INIT(IO_ESP_MISO,    IO_MODE_INPUT_PULL);    IO_HIGH(IO_ESP_MISO);
        IO_INIT(IO_ESP_MOSI,    IO_MODE_OUTPUT_APP);    IO_HIGH(IO_ESP_MOSI);
        IO_INIT(IO_DS_WIRE,     IO_MODE_OUTPUT_APP);
        IO_INIT(IO_TPWM,        IO_MODE_OUTPUT_APP);
        IO_INIT(IO_LS_SCL,      IO_MODE_OUTPUT_AOD);
        IO_INIT(IO_LS_SDA,      IO_MODE_OUTPUT_AOD);
        IO_INIT(IO_SPWM,        IO_MODE_OUTPUT_APP);
        IO_INIT(IO_TSELP,       IO_MODE_OUTPUT_PP);
        IO_INIT(IO_TSELC0,      IO_MODE_OUTPUT_PP);
        IO_INIT(IO_TSELC1,      IO_MODE_OUTPUT_PP);
        IO_INIT(IO_TSELC2,      IO_MODE_OUTPUT_PP);
        IO_INIT(IO_TSELC3,      IO_MODE_OUTPUT_PP);
    IO_SAVE(B);
    // Порт C
    IO_LOAD(C);
        IO_INIT(IO_ESP_RST,     IO_MODE_OUTPUT_PP);
        IO_INIT(IO_OSC32_IN,    IO_MODE_INPUT_PULL);
        IO_INIT(IO_OSC32_OUT,   IO_MODE_INPUT_PULL);
    IO_SAVE(C);
    // Порт D
    IO_LOAD(D);
        IO_INIT(IO_OSC_IN,      IO_MODE_INPUT_PULL);
        IO_INIT(IO_OSC_OUT,     IO_MODE_INPUT_PULL);
    IO_SAVE(D);

    // Ремап
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;                                         // AFIO clock enable
    AFIO->MAPR = AFIO_MAPR_TIM2_REMAP_PARTIALREMAP2 |                           // TIM2 partial remap (CH1/ETR/PA0, CH2/PA1, CH3/PB10, CH4/PB11)
                 AFIO_MAPR_SPI1_REMAP |                                         // SPI1 remap (NSS/PA15, SCK/PB3, MISO/PB4, MOSI/PB5)
                 AFIO_MAPR_SWJ_CFG_1;                                           // Only SWD w/o SWO
}
