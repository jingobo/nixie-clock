#include "io.h"
#include "core.h"
#include "task.h"
#include "event.h"
#include "ntime.h"

// Номера модулей SPI
#define SPI             0
#define HSPI            1
// Адрес статусного регистра прерываний SPI (HSPI, I2S)
#define IRQ_SRC_REG     0x3ff00020
// Источники прерывания
#define IRQ_SRC_SPI     BIT4
#define IRQ_SRC_HSPI    BIT7
#define IRQ_SRC_I2S     BIT9
// Имя модуля для логирования
#define MODULE_NAME     "CORE"

#ifndef NDEBUG
    // Отладочный вывод содержимого SPI регистров
    ROM static void core_spi_debug(void)
    {
        // Основные регистры
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
            log_module(MODULE_NAME, "ADDR[0x%08x], Value[0x%08x]", reg, READ_PERI_REG(reg));
        }
    }
    // Отладочный вывод
    #define CORE_SPI_DEBUG()    core_spi_debug()
#else // NDEBUG
    // Отладочный вывод
    #define CORE_SPI_DEBUG()    BLOCK_EMPTY
#endif // NDEBUG

// Контроллер пакетов
static class core_t : public ipc_controller_slave_t, public task_wrapper_t
{
    // Буфер для пакетов приёма/передачи
    union
    {
        // Пакеты приёма/передачи
        struct
        {
            ipc_packet_t rx, tx;
        };
        // Для FIFO
        uint32_t raw[IPC_PKT_SIZE  / sizeof(uint32_t) * 2];
    } buffer;

    // Данные последнего полученного пакета TODO: нужно нормально рассчитать длинну массива
    uint8_t data[256];
    // Событие готовности данных
    event_auto_t event_data_ready;

    // Выполнение транзакции
    void transaction(void);
    // Проверяет, что пакет обрабатывать не нам
    static bool is_our_packet(const ipc_packet_t &packet);
protected:
    // Обработчик события подготовки к получению данных (получение буфера и печенек)
    virtual bool receive_prepare(const ipc_packet_t &packet, receive_args_t &args);
    // Обработчик события завершения получения данных (пакеты собраны)
    virtual bool receive_finalize(const ipc_packet_t &packet, const receive_args_t &args);
    // Полный сброс прикладного уровня
    virtual void reset_layer(ipc_command_flow_request_t::reason_t reason, bool internal = true);
    // Добавление данных к передаче
    virtual bool transmit_raw(ipc_dir_t dir, ipc_command_t command, const void *source, size_t size);
    // Обработчик задачи
    virtual void execute(void);
public:
    // Конструктор по умолчанию
    core_t(void) : task_wrapper_t("core")
    { }

    // Добавление обработчика событий
    virtual void handler_add_event(ipc_handler_event_t &handler);
    // Добавление обработчика команд
    virtual void handler_add_command(ipc_handler_command_t &handler);

    // Получение пакета к выводу
    virtual void packet_output(ipc_packet_t &packet);
    // Ввод полученного пакета
    virtual void packet_input(const ipc_packet_t &packet);

    // Обработчик прерывания от HSPI
    static void spi_isr(void *dummy);
} core;

RAM bool core_t::is_our_packet(const ipc_packet_t &packet)
{
    return  packet.dll.dir != IPC_DIR_REQUEST && packet.dll.command < IPC_COMMAND_ESP_HANDLE_BASE;
}

ROM bool core_t::receive_prepare(const ipc_packet_t &packet, receive_args_t &args)
{
    // Индикация
    IO_LED_YELLOW.flash();
    // Если не запрос - значит не нам
    if (is_our_packet(packet))
    {
        args.buffer = data;
        // TODO: отправка вебсокету
        return true;
    }
    // Базовый обработчик
    return ipc_controller_slave_t::receive_prepare(packet, args);
}

ROM bool core_t::receive_finalize(const ipc_packet_t &packet, const receive_args_t &args)
{
    // Если не запрос - значит не нам
    if (is_our_packet(packet))
    {
        log_module(MODULE_NAME, "Web response 0x%02x, %d bytes", packet.dll.command, args.size);
        // TODO: отправка вебсокету
        return true;
    }
    log_module(MODULE_NAME, "Esp request 0x%02x, %d bytes", packet.dll.command, args.size);
    // Базовый обработчик
    return ipc_controller_slave_t::receive_finalize(packet, args);
}

ROM void core_t::reset_layer(ipc_command_flow_request_t::reason_t reason, bool internal)
{
    // Базовый метод
    ipc_controller_slave_t::reset_layer(reason, internal);
    // Вывод в лог
    log_module(MODULE_NAME, "Layer reset, reason %d, internal %d", reason, internal);
}

ROM bool core_t::transmit_raw(ipc_dir_t dir, ipc_command_t command, const void *source, size_t size)
{
    bool result;
    MUTEX_ENTER(mutex);
        result = ipc_controller_slave_t::transmit_raw(dir, command, source, size);
    MUTEX_LEAVE(mutex);
    return result;
}

ROM void core_t::execute(void)
{
    TASK_PRIORITY_SET(TASK_PRIORITY_CRITICAL);
    // SPI enable
    SET_PERI_REG_MASK(SPI_CMD(HSPI), SPI_USR);
    for (;;)
        transaction();
}

RAM void core_t::handler_add_event(ipc_handler_event_t &handler)
{
    MUTEX_ENTER(mutex);
        ipc_controller_slave_t::handler_add_event(handler);
    MUTEX_LEAVE(mutex);
}

RAM void core_t::handler_add_command(ipc_handler_command_t &handler)
{
    MUTEX_ENTER(mutex);
        ipc_controller_slave_t::handler_add_command(handler);
    MUTEX_LEAVE(mutex);
}

RAM void core_t::packet_output(ipc_packet_t &packet)
{
    MUTEX_ENTER(mutex);
        ipc_controller_slave_t::packet_output(packet);
    MUTEX_LEAVE(mutex);
}

RAM void core_t::packet_input(const ipc_packet_t &packet)
{
    MUTEX_ENTER(mutex);
        ipc_controller_slave_t::packet_input(packet);
    MUTEX_LEAVE(mutex);
}

ROM void core_t::transaction(void)
{
    // Вывод пакета
    packet_output(buffer.tx);
    // В регистры
    for (auto i = 8; i < 16; i++)
        WRITE_PERI_REG(SPI_W0(HSPI) + i * REG_SIZE, buffer.raw[i]);
    // Ожидание транзакции
    event_data_ready.wait();
    // Чтение из регистров
    for (auto i = 0; i < 8; i++)
        buffer.raw[i] = READ_PERI_REG(SPI_W0(HSPI) + i * REG_SIZE);
    // Ввод пакета
    packet_input(buffer.rx);
}

RAM void core_t::spi_isr(void *dummy)
{
    auto sr = READ_PERI_REG(IRQ_SRC_REG);
    // SPI
    if (sr & IRQ_SRC_SPI)
        // Сброс флагов, источников прерывания
        CLEAR_PERI_REG_MASK(SPI_SLAVE(SPI), 0x3ff);
    // HSPI
    if (!(sr & IRQ_SRC_HSPI))
        return;
    // Сброс флага прерывания (не знаю почему у китайцев так через жопу)
    CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_TRANS_DONE_EN);
        SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SYNC_RESET);
        CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_TRANS_DONE);
    SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_TRANS_DONE_EN);
    // Вызов события
    core.event_data_ready.set_isr();
}

RAM bool core_transmit(ipc_dir_t dir, const ipc_command_data_t &data)
{
    return core.transmit(dir, data);
}

RAM void core_handler_add_event(ipc_handler_event_t &handler)
{
    core.handler_add_event(handler);
}

RAM void core_handler_add_command(ipc_handler_command_t &handler)
{
    core.handler_add_command(handler);
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
    // Настройка прерывание HSPI
    _xt_isr_attach(ETS_SPI_INUM, core_t::spi_isr, NULL);
    _xt_isr_unmask(1 << ETS_SPI_INUM);

    // Проверка размера пакета
    IPC_PKT_SIZE_CHECK();
    // Запуск задачи обработки пакетов
    core.start();
    assert(core.running());
}
