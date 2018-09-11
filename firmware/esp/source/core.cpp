#include "io.h"
#include "core.h"
#include "task.h"
#include "event.h"
#include "ntime.h"

// ������ ������� SPI
#define SPI             0
#define HSPI            1
// ����� ���������� �������� ���������� SPI (HSPI, I2S)
#define IRQ_SRC_REG     0x3ff00020
// ��������� ����������
#define IRQ_SRC_SPI     BIT4
#define IRQ_SRC_HSPI    BIT7
#define IRQ_SRC_I2S     BIT9
// ��� ������ ��� �����������
#define MODULE_NAME     "CORE"

#ifndef NDEBUG
    // ���������� ����� ����������� SPI ���������
    ROM static void core_spi_debug(void)
    {
        // �������� ��������
        log_module(MODULE_NAME, "SPI_CMD       [0x%08x]", READ_PERI_REG(SPI_CMD(HSPI)));
        log_module(MODULE_NAME, "SPI_CTRL      [0x%08x]", READ_PERI_REG(SPI_CTRL(HSPI)));
        log_module(MODULE_NAME, "SPI_CTRL2     [0x%08x]", READ_PERI_REG(SPI_CTRL2(HSPI)));
        log_module(MODULE_NAME, "SPI_CLOCK     [0x%08x]", READ_PERI_REG(SPI_CLOCK(HSPI)));
        log_module(MODULE_NAME, "SPI_USER      [0x%08x]", READ_PERI_REG(SPI_USER(HSPI)));
        log_module(MODULE_NAME, "SPI_USER1     [0x%08x]", READ_PERI_REG(SPI_USER1(HSPI)));
        log_module(MODULE_NAME, "SPI_USER2     [0x%08x]", READ_PERI_REG(SPI_USER2(HSPI)));
        log_module(MODULE_NAME, "SPI_PIN       [0x%08x]", READ_PERI_REG(SPI_PIN(HSPI)));
        log_module(MODULE_NAME, "SPI_SLAVE     [0x%08x]", READ_PERI_REG(SPI_SLAVE(HSPI)));
        log_module(MODULE_NAME, "SPI_SLAVE1    [0x%08x]", READ_PERI_REG(SPI_SLAVE1(HSPI)));
        log_module(MODULE_NAME, "SPI_SLAVE2    [0x%08x]", READ_PERI_REG(SPI_SLAVE2(HSPI)));
        // FIFO
        for (auto i = 0; i < 16; ++i)
        {
            auto reg = SPI_W0(HSPI) + i * 4;
            log_module(MODULE_NAME, "ADDR[0x%08x],Value[0x%08x]", reg, READ_PERI_REG(reg));
        }
    }
    // ���������� �����
    #define CORE_SPI_DEBUG()    core_spi_debug()
#else // NDEBUG
    // ���������� �����
    #define CORE_SPI_DEBUG()    BLOCK_EMPTY
#endif // NDEBUG

// ���������� �������
static class core_t : public ipc_controller_t, public task_wrapper_t
{
    // ����� ��� ������� �����/��������
    union
    {
        // ������ �����/��������
        struct
        {
            ipc_packet_t rx, tx;
        };
        // ��� FIFO
        uint32_t raw[IPC_PKT_SIZE  / sizeof(uint32_t) * 2];
    } buffer;

    // ������ ���������� ����������� ������ TODO: ����� ��������� ���������� ������ �������
    uint8_t data[256];
    // ������� ���������� ������
    event_auto_t event_data_ready;

    // ���������� ����������
    void transaction(void);
protected:
    // ���������� ������� ���������� � ��������� ������ (��������� ������ � �������)
    virtual bool receive_prepare(const ipc_packet_t &packet, receive_args_t &args);
    // ���������� ������� ���������� ��������� ������ (������ �������)
    virtual bool receive_finalize(const ipc_packet_t &packet, const receive_args_t &args);
    // ������ ����� ����������� ������
    virtual void reset_layer(ipc_command_flow_t::reason_t reason, bool internal = true);
    // ���������� ������
    virtual void execute(void);
public:
    // ����������� �� ���������
    core_t(void) : task_wrapper_t("core")
    { }

    // ���������� ���������� �� HSPI
    static void spi_isr(void *dummy);
} core;

ROM bool core_t::receive_prepare(const ipc_packet_t &packet, receive_args_t &args)
{
    // ���������
    IO_LED_YELLOW.flash();
    // ���� �� ������ - ������ �� ���
    if (packet.dll.dir != IPC_DIR_REQUEST)
    {
        args.buffer = data;
        // TODO: �������� ���������
        return true;
    }
    // ������� ����������
    return ipc_controller_t::receive_prepare(packet, args);
}

ROM bool core_t::receive_finalize(const ipc_packet_t &packet, const receive_args_t &args)
{
    // ���� �� ������ - ������ �� ���
    if (packet.dll.dir != IPC_DIR_REQUEST)
    {
        log_module(MODULE_NAME, "Web response 0x%02x, %d bytes", packet.dll.command, args.size);
        // TODO: �������� ���������
        return true;
    }
    log_module(MODULE_NAME, "Esp request 0x%02x, %d bytes", packet.dll.command, args.size);
    // ������� ����������
    return ipc_controller_t::receive_finalize(packet, args);
}

ROM void core_t::reset_layer(ipc_command_flow_t::reason_t reason, bool internal)
{
    // ������� �����
    ipc_controller_t::reset_layer(reason, internal);
    // ����� � ���
    log_module(MODULE_NAME, "Layer reset, reason %d, internal %d", reason, internal);
}

ROM void core_t::execute(void)
{
    // SPI enable
    SET_PERI_REG_MASK(SPI_CMD(HSPI), SPI_USR);
    for (;;)
        transaction();
}

ROM void core_t::transaction(void)
{
    // ����� ������
    MUTEX_ENTER(mutex);
        packet_output(buffer.tx);
    MUTEX_LEAVE(mutex);
    // � ��������
    for (auto i = 8; i < 16; i++)
        WRITE_PERI_REG(SPI_W0(HSPI) + i * REG_SIZE, buffer.raw[i]);
    // �������� ����������
    event_data_ready.wait();
    // ������ �� ���������
    for (auto i = 0; i < 8; i++)
        buffer.raw[i] = READ_PERI_REG(SPI_W0(HSPI) + i * REG_SIZE);
    // ���� ������
    MUTEX_ENTER(mutex);
        packet_input(buffer.rx);
    MUTEX_LEAVE(mutex);
}

RAM void core_t::spi_isr(void *dummy)
{
    auto sr = READ_PERI_REG(IRQ_SRC_REG);
    // SPI
    if (sr & IRQ_SRC_SPI)
        // ����� ������, ���������� ����������
        CLEAR_PERI_REG_MASK(SPI_SLAVE(SPI), 0x3ff);
    // HSPI
    if (!(sr & IRQ_SRC_HSPI))
        return;
    // ����� ����� ���������� (�� ���� ������ � �������� ��� ����� ����)
    CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_TRANS_DONE_EN);
        SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SYNC_RESET);
        CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_TRANS_DONE);
    SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_TRANS_DONE_EN);
    // ����� ��������� TODO: �������� ������
    for (auto i = 8; i < 16; i++)
        WRITE_PERI_REG(SPI_W0(HSPI) + i * REG_SIZE, 0xFFFFFFFF);
    // ����� �������
    core.event_data_ready.set_isr();
}


ROM bool core_transmit(ipc_dir_t dir, const ipc_command_data_t &data)
{
    bool result;
    MUTEX_ENTER(core.mutex);
        result = core.transmit(dir, data);
    MUTEX_LEAVE(core.mutex);
    return result;
}

ROM void core_handler_add_command(ipc_handler_command_t &handler)
{
    MUTEX_ENTER(core.mutex);
        core.handler_add_command(handler);
    MUTEX_LEAVE(core.mutex);
}

ROM void core_init(void)
{
    assert(!core.running());
    // HSPI GPIO
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_HSPIQ_MISO);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_HSPID_MOSI);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_HSPI_CLK);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_HSPI_CS0);

    // CTRL (MSB)
    WRITE_PERI_REG(SPI_CTRL(HSPI), 0);
    // SLAVE
    WRITE_PERI_REG(SPI_SLAVE(HSPI), SPI_SLAVE_MODE | SPI_SLV_WR_RD_BUF_EN | SPI_TRANS_DONE_EN);
    // USER (Half-Duplex, CHPA first, MISO on FIFO bottom)
    WRITE_PERI_REG(SPI_USER(HSPI), SPI_CK_I_EDGE | SPI_USR_MISO_HIGHPART);
    // CLOCK (reset)
    WRITE_PERI_REG(SPI_CLOCK(HSPI), 0);

    SET_PERI_REG_BITS(SPI_CTRL2(HSPI), SPI_MISO_DELAY_MODE, 1, SPI_MISO_DELAY_MODE_S);

    // USER1 (8-bit address phase)
    WRITE_PERI_REG(SPI_USER1(HSPI), 7 << SPI_USR_ADDR_BITLEN_S);
    // USER2 (8-bit command phase)
    WRITE_PERI_REG(SPI_USER2(HSPI), 7 << SPI_USR_COMMAND_BITLEN_S);
    // SLAVE1 (slave buffer size 32 byte)
    WRITE_PERI_REG(SPI_SLAVE1(HSPI), ((32 * 8 - 1) << SPI_SLV_BUF_BITLEN_S));
    // PIN (CPOL low, CS)
    WRITE_PERI_REG(SPI_PIN(HSPI), BIT19);
    // ��������� ���������� HSPI
    _xt_isr_attach(ETS_SPI_INUM, core_t::spi_isr, NULL);
    _xt_isr_unmask(1 << ETS_SPI_INUM);
    // ��� �������
    //CORE_SPI_DEBUG();

    // �������� ������� ������
    IPC_PKT_SIZE_CHECK();
    // ������ ������ ��������� �������
    core.start();
    assert(core.running());
}
