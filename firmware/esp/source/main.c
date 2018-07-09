#include "io.h"

#define HSPI 				1

static void main_write_test(void)
{
	uint8_t i;
	for (i = 8; i < 16; i++)
		WRITE_PERI_REG(SPI_W0(HSPI) + i * 4, 0xF0F0F0F0);
}

void __ShowRegValu1e(void)
{

    int i;
    uint32_t regAddr = 0x60000140;

    os_printf(" SPI_ADDR      [0x%08x]\r\n", READ_PERI_REG(SPI_ADDR(HSPI)));
    os_printf(" SPI_CMD       [0x%08x]\r\n", READ_PERI_REG(SPI_CMD(HSPI)));
    os_printf(" SPI_CTRL      [0x%08x]\r\n", READ_PERI_REG(SPI_CTRL(HSPI)));
    os_printf(" SPI_CTRL2     [0x%08x]\r\n", READ_PERI_REG(SPI_CTRL2(HSPI)));
    os_printf(" SPI_CLOCK     [0x%08x]\r\n", READ_PERI_REG(SPI_CLOCK(HSPI)));
    os_printf(" SPI_RD_STATUS [0x%08x]\r\n", READ_PERI_REG(SPI_RD_STATUS(HSPI)));
    os_printf(" SPI_WR_STATUS [0x%08x]\r\n", READ_PERI_REG(SPI_WR_STATUS(HSPI)));
    os_printf(" SPI_USER      [0x%08x]\r\n", READ_PERI_REG(SPI_USER(HSPI)));
    os_printf(" SPI_USER1     [0x%08x]\r\n", READ_PERI_REG(SPI_USER1(HSPI)));
    os_printf(" SPI_USER2     [0x%08x]\r\n", READ_PERI_REG(SPI_USER2(HSPI)));
    os_printf(" SPI_PIN       [0x%08x]\r\n", READ_PERI_REG(SPI_PIN(HSPI)));
    os_printf(" SPI_SLAVE     [0x%08x]\r\n", READ_PERI_REG(SPI_SLAVE(HSPI)));
    os_printf(" SPI_SLAVE1    [0x%08x]\r\n", READ_PERI_REG(SPI_SLAVE1(HSPI)));
    os_printf(" SPI_SLAVE2    [0x%08x]\r\n", READ_PERI_REG(SPI_SLAVE2(HSPI)));
    os_printf(" SPI_SLAVE3    [0x%08x]\r\n", READ_PERI_REG(SPI_SLAVE3(HSPI)));

    for (i = 0; i < 16; ++i) {
        os_printf(" ADDR[0x%08x],Value[0x%08x]\r\n", regAddr, READ_PERI_REG(regAddr));
        regAddr += 4;
    }

}

static void ICACHE_RAM_ATTR main_spi_irq_clear(void)
{
	CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SLV_WR_STA_DONE | SPI_SLV_RD_STA_DONE | SPI_SLV_WR_BUF_DONE | SPI_SLV_RD_BUF_DONE | SPI_TRANS_DONE);
}

void ICACHE_RAM_ATTR main_spi_isr(void *dummy)
{
    uint32 regvalue;
    uint32 statusW, statusR, counter;
    if (READ_PERI_REG(0x3ff00020)&BIT4) {
        //following 3 lines is to clear isr signal
        CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), 0x3ff);
    } else if (READ_PERI_REG(0x3ff00020)&BIT7) { //bit7 is for hspi isr,
        regvalue = READ_PERI_REG(SPI_SLAVE(HSPI));
        os_printf("spi_slave_isr_sta SPI_SLAVE[0x%08x]\n\r", regvalue);
        main_spi_irq_clear();
        SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SYNC_RESET);
        main_spi_irq_clear();

        // SLAVE
        WRITE_PERI_REG(SPI_SLAVE(HSPI), SPI_SLAVE_MODE | SPI_SLV_WR_RD_BUF_EN | SPI_SLV_RD_BUF_DONE_EN);

        if (regvalue & SPI_SLV_WR_BUF_DONE) {
            // User can get data from the W0~W7
            os_printf("spi_slave_isr_sta : SPI_SLV_WR_BUF_DONE\n\r");
        } else if (regvalue & SPI_SLV_RD_BUF_DONE) {
            // TO DO
            os_printf("spi_slave_isr_sta : SPI_SLV_RD_BUF_DONE\n\r");
        }
        if (regvalue & SPI_SLV_RD_STA_DONE) {
            statusR = READ_PERI_REG(SPI_RD_STATUS(HSPI));
            statusW = READ_PERI_REG(SPI_WR_STATUS(HSPI));
            os_printf("spi_slave_isr_sta : SPI_SLV_RD_STA_DONE[R=0x%08x,W=0x%08x]\n\r", statusR, statusW);
        }

        if (regvalue & SPI_SLV_WR_STA_DONE) {
            statusR = READ_PERI_REG(SPI_RD_STATUS(HSPI));
            statusW = READ_PERI_REG(SPI_WR_STATUS(HSPI));
            os_printf("spi_slave_isr_sta : SPI_SLV_WR_STA_DONE[R=0x%08x,W=0x%08x]\n\r", statusR, statusW);
        }
        if ((regvalue & SPI_TRANS_DONE) && ((regvalue & 0xf) == 0)) {
            os_printf("spi_slave_isr_sta : SPI_TRANS_DONE\n\r");

        }
    }
}

void ICACHE_FLASH_ATTR main_init(void)
{
    // Инициализация GPIO
    WRITE_PERI_REG(PERIPHS_IO_MUX_CONF_U, 0x105);
    // LED
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4); GPIO_ENA_SET(4, true); GPIO_OUT_SET(4, false);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5); GPIO_ENA_SET(5, true);	GPIO_OUT_SET(5, false);
    // HSPI
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_HSPIQ_MISO);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_HSPID_MOSI);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_HSPI_CLK);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_HSPI_CS0);

    // CTRL (MSB)
    WRITE_PERI_REG(SPI_CTRL(HSPI), 0);

    // SLAVE
    WRITE_PERI_REG(SPI_SLAVE(HSPI), SPI_SLAVE_MODE | SPI_SLV_WR_RD_BUF_EN | SPI_SLV_RD_BUF_DONE_EN);

    // USER (Duplex, CHPA first, MOSI MISO phase)
    WRITE_PERI_REG(SPI_USER(HSPI), SPI_CK_I_EDGE | SPI_USR_MISO_HIGHPART);

    // CLOCK (reset?)
    WRITE_PERI_REG(SPI_CLOCK(HSPI), 0);

    // USER1 (MOSI, MISO 8 words)
    WRITE_PERI_REG(SPI_USER1(HSPI), 7 << SPI_USR_ADDR_BITLEN_S);

    // USER2 (MOSI, MISO 8 words)
    WRITE_PERI_REG(SPI_USER2(HSPI), 7 << SPI_USR_COMMAND_BITLEN_S);

    // SLAVE1 (reset?)
    WRITE_PERI_REG(SPI_SLAVE1(HSPI), ((31 * 8 - 1) << SPI_SLV_BUF_BITLEN_S) | SPI_SLV_RDBUF_DUMMY_EN);

    // SLAVE2 (reset?)
    WRITE_PERI_REG(SPI_SLAVE2(HSPI), 7 << SPI_SLV_RDBUF_DUMMY_CYCLELEN_S);


    // PIN (CPOL low, CS)
    WRITE_PERI_REG(SPI_PIN(HSPI), BIT19);


    main_spi_irq_clear();

    // Attach ISR
    //_xt_isr_attach(ETS_SPI_INUM, main_spi_isr, NULL);
	// IRQ enable
    //_xt_isr_unmask(1 << ETS_SPI_INUM);

    // CTRL2 (MOSI delay mode 2 - SPI mode 0)
    //WRITE_PERI_REG(SPI_CTRL2(HSPI), (0x02 << SPI_MOSI_DELAY_MODE_S) | (0x01 << SPI_MISO_DELAY_MODE_S));

    SET_PERI_REG_MASK(SPI_CMD(HSPI), SPI_USR);
	do
	{
		uint32_t i;

		main_write_test();

	    /*for (i = 0; i < 0xFF0000; i++)
	    {
	    	if (READ_PERI_REG(SPI_SLAVE(HSPI)) & SPI_SLV_RD_BUF_DONE)
	    		continue;
	    	CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SLV_RD_BUF_DONE);
		    printf("Catch");
		    break;
	    }*/

		while (!(READ_PERI_REG(SPI_SLAVE(HSPI)) & SPI_SLV_RD_BUF_DONE))
		{ }
    	CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SLV_RD_BUF_DONE);

    	__ShowRegValu1e();
	} while (true);



	//CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SLV_WR_STA_DONE_EN | SPI_SLV_RD_STA_DONE_EN | SPI_SLV_WR_BUF_DONE_EN | SPI_SLV_RD_BUF_DONE_EN | SPI_TRANS_DONE_EN);
	//main_spi_irq_clear();


    // Listen


	/*// Инициализация SPI
    	// CPOL CPHA
    //CLEAR_PERI_REG_MASK(SPI_PIN(HSPI), SPI_IDLE_EDGE);
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_CK_OUT_EDGE);
    	// MSB
    CLEAR_PERI_REG_MASK(SPI_CTRL(HSPI), SPI_WR_BIT_ORDER);
    CLEAR_PERI_REG_MASK(SPI_CTRL(HSPI), SPI_RD_BIT_ORDER);
    // Operation mode
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_FLASH_MODE);
    SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SLAVE_MODE);
    SET_PERI_REG_MASK(SPI_PIN(HSPI), SPI_CS_EDGE);

    // SPI TX
    SET_PERI_REG_MASK(SPI_USER(HSPI), SPI_USR_MISO_HIGHPART); // C8-C15
    //
    SET_PERI_REG_MASK(SPI_USER(HSPI), SPI_USR_MOSI);

    // If do not set delay cycles, slave not working,master cann't get the data.
    SET_PERI_REG_MASK(SPI_CTRL2(HSPI), ((0x1 & SPI_MOSI_DELAY_NUM) << SPI_MOSI_DELAY_NUM_S)); //delay num
    // SPI Speed
    WRITE_PERI_REG(SPI_CLOCK(HSPI), 0);

    // By default format::CMD(8bits)+ADDR(8bits)+DATA(32bytes).
    SET_PERI_REG_BITS(SPI_USER2(HSPI), SPI_USR_COMMAND_BITLEN,
                      7, SPI_USR_COMMAND_BITLEN_S);
    SET_PERI_REG_BITS(SPI_SLAVE1(HSPI), SPI_SLV_WR_ADDR_BITLEN,
                      7, SPI_SLV_WR_ADDR_BITLEN_S);
    SET_PERI_REG_BITS(SPI_SLAVE1(HSPI), SPI_SLV_RD_ADDR_BITLEN,
                      7, SPI_SLV_RD_ADDR_BITLEN_S);
    SET_PERI_REG_BITS(SPI_SLAVE1(HSPI), SPI_SLV_BUF_BITLEN,
                      (32 * 8 - 1), SPI_SLV_BUF_BITLEN_S);
    // For 8266 work on slave mode.
    SET_PERI_REG_BITS(SPI_SLAVE1(HSPI), SPI_SLV_STATUS_BITLEN,
                      7, SPI_SLV_STATUS_BITLEN_S);

    main_write_test();


    //SPIIntDisable(SpiNum_HSPI, SpiIntSrc_WrStaDoneEn
                 //| SpiIntSrc_RdStaDoneEn | SpiIntSrc_WrBufDoneEn | SpiIntSrc_RdBufDoneEn);
    //SPIIntEnable(SpiNum_HSPI, SpiIntSrc_TransDoneEn);

    SET_PERI_REG_MASK(SPI_CMD(HSPI), SPI_USR);
    _xt_isr_attach(ETS_SPI_INUM, main_spi_isr, NULL);
    _xt_isr_unmask(1 << ETS_SPI_INUM);*/

    /*SpiAttr spi =
	{
		.mode = SpiMode_Slave,
		.subMode = SpiSubMode_0,
		.speed = SpiSpeed_20MHz,
		.bitOrder = SpiBitOrder_MSBFirst
	};
	SPIInit(SpiNum_HSPI, &spi);


	SPISlaveRecvData(SpiNum_HSPI, main_spi_isr);
	//SPIIntEnable(SpiNum_HSPI, SpiIntSrc_TransDoneEn);
    main_write_test();
    WRITE_PERI_REG(SPI_RD_STATUS(SpiNum_HSPI), 0x8A);
    WRITE_PERI_REG(SPI_WR_STATUS(SpiNum_HSPI), 0x83); */

}
