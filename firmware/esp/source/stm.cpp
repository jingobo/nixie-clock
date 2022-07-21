#include "os.h"
#include "log.h"
#include "stm.h"
#include "core.h"

// Номера модуля SPI
#define STM_SPI             0
#define STM_HSPI            1

// Имя модуля для логирования
LOG_TAG_DECL("STM");

#if 0 && !defined NDEBUG
    // Отладочный вывод содержимого SPI регистров
    static void stm_spi_debug(void)
    {
        // Основные регистры
        LOGI("SPI_CMD       [0x%08x]", READ_PERI_REG(SPI_CMD(STM_HSPI)));
        LOGI("SPI_CTRL      [0x%08x]", READ_PERI_REG(SPI_CTRL(STM_HSPI)));
        LOGI("SPI_CTRL2     [0x%08x]", READ_PERI_REG(SPI_CTRL2(STM_HSPI)));
        LOGI("SPI_CLOCK     [0x%08x]", READ_PERI_REG(SPI_CLOCK(STM_HSPI)));
        LOGI("SPI_USER      [0x%08x]", READ_PERI_REG(SPI_USER(STM_HSPI)));
        LOGI("SPI_USER1     [0x%08x]", READ_PERI_REG(SPI_USER1(STM_HSPI)));
        LOGI("SPI_USER2     [0x%08x]", READ_PERI_REG(SPI_USER2(STM_HSPI)));
        LOGI("SPI_PIN       [0x%08x]", READ_PERI_REG(SPI_PIN(STM_HSPI)));
        LOGI("SPI_SLAVE     [0x%08x]", READ_PERI_REG(SPI_SLAVE(STM_HSPI)));
        LOGI("SPI_SLAVE1    [0x%08x]", READ_PERI_REG(SPI_SLAVE1(STM_HSPI)));
        LOGI("SPI_SLAVE2    [0x%08x]", READ_PERI_REG(SPI_SLAVE2(STM_HSPI)));
        // FIFO
        for (auto i = 0; i < 16; ++i)
        {
            auto reg = SPI_W0(STM_HSPI) + i * 4;
            LOGI("ADDR[0x%08x], Value[0x%08x]", reg, READ_PERI_REG(reg));
        }
    }
    // Отладочный вывод
    #define STM_SPI_DEBUG()     stm_spi_debug()
#else // NDEBUG
    // Отладочный вывод
    #define STM_SPI_DEBUG()     EMPTY_BLOCK
#endif // NDEBUG

// Задача обслуживания SPI
static class stm_task_t : public os_task_base_t
{
protected:
    // Обработчик задачи
    virtual void execute(void);
public:
    // Конструктор по умолчанию
    stm_task_t(void) : os_task_base_t("stm", true)
    { }
} stm_task;

// Связь с STM
static class stm_link_t : public ipc_link_t
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
    // Событие готовности данных
    os_event_auto_t event_data_ready;
protected:
    // Полный сброс прикладного уровня
    virtual void reset_layer(reset_reason_t reason, bool internal = true)
    {
        // Базовый метод
        ipc_link_t::reset_layer(reason, internal);
        // Вывод в лог
        LOGW("Layer reset, reason %d, internal %d", reason, internal);
    }
public:
    // Событие простоя линии связи
    os_event_auto_t event_spi_idle;

    // Обработка пакета (перенос в передачу)
    virtual bool packet_process(const ipc_packet_t &packet, const args_t &args) override final;

    // Выполнение транзакции
    void transaction(void);
    // Обработчик прерывания от HSPI
    static void spi_isr(void *dummy);
} stm_link;

RAM bool stm_link_t::packet_process(const ipc_packet_t &packet, const args_t &args)
{
    // Начало ввода
    if (args.first)
        stm_task.mutex.enter();

    // Ввод
    auto result = ipc_link_t::packet_process(packet, args);

    // Завершение ввода
    if (!(result && packet.dll.more))
        stm_task.mutex.leave();
    return result;
}

// Выполнение транзакции
RAM void stm_link_t::transaction(void)
{
    // Вывод пакета
    stm_task.mutex.enter();
        packet_output(buffer.tx);
    stm_task.mutex.leave();

    // Запись в регистры
    taskENTER_CRITICAL();
        for (auto i = 8; i < 16; i++)
            WRITE_PERI_REG(SPI_W0(STM_HSPI) + i * SYSTEM_REG_SIZE, buffer.raw[i]);
    taskEXIT_CRITICAL();

    // Ожидание транзакции
    event_spi_idle.set();
    event_data_ready.wait();

    // Чтение из регистров
    taskENTER_CRITICAL();
        for (auto i = 0; i < 8; i++)
            buffer.raw[i] = READ_PERI_REG(SPI_W0(STM_HSPI) + i * SYSTEM_REG_SIZE);
    taskEXIT_CRITICAL();

    // Ввод пакета
    stm_task.mutex.enter();
        auto result = packet_input(buffer.rx);
    stm_task.mutex.leave();

    // Обработка
    if (result && !buffer.rx.dll.more)
        // Синхронизация на приватный мьютекс не требуется
        flush_packets(core_processor_out.stm);
}

// Адрес статусного регистра прерываний SPI (HSPI, I2S)
#define STM_IRQ_SRC_REG     0x3ff00020
// Источники прерывания
#define STM_IRQ_SRC_SPI     BIT4
#define STM_IRQ_SRC_HSPI    BIT7

RAM void stm_link_t::spi_isr(void *dummy)
{
    auto sr = READ_PERI_REG(STM_IRQ_SRC_REG);
    // SPI
    if (sr & STM_IRQ_SRC_SPI)
        // Сброс флагов, источников прерывания
        CLEAR_PERI_REG_MASK(SPI_SLAVE(STM_SPI), 0x3ff);
    // HSPI
    if (!(sr & STM_IRQ_SRC_HSPI))
        return;
    // Сброс флага прерывания (не знаю почему у китайцев так через жопу)
    CLEAR_PERI_REG_MASK(SPI_SLAVE(STM_HSPI), SPI_TRANS_DONE_EN);
        SET_PERI_REG_MASK(SPI_SLAVE(STM_HSPI), SPI_SYNC_RESET);
        CLEAR_PERI_REG_MASK(SPI_SLAVE(STM_HSPI), SPI_TRANS_DONE);
    SET_PERI_REG_MASK(SPI_SLAVE(STM_HSPI), SPI_TRANS_DONE_EN);
    // Вызов события
    stm_link.event_data_ready.set_isr();
}

RAM void stm_task_t::execute(void)
{
    priority_set(OS_TASK_PRIORITY_CRITICAL);
    // SPI enable
    SET_PERI_REG_MASK(SPI_CMD(STM_HSPI), SPI_USR);
    // Цико опроса
    for (;;)
        stm_link.transaction();
}

// Процессор входящих пактов для STM
ipc_processor_proxy_t stm_processor_in([](const ipc_packet_t &packet, const ipc_processor_t::args_t &args)
{
    return stm_link.packet_process(packet, args);
});

void stm_init(void)
{
    // HSPI GPIO
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_HSPIQ_MISO);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_HSPID_MOSI);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_HSPI_CLK);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_HSPI_CS0);

    // CTRL (MSB)
    WRITE_PERI_REG(SPI_CTRL(STM_HSPI), 0);
    // SLAVE
    WRITE_PERI_REG(SPI_SLAVE(STM_HSPI), SPI_SLAVE_MODE | SPI_SLV_WR_RD_BUF_EN | SPI_TRANS_DONE_EN);
    // USER (Half-Duplex, CHPA first, MISO on FIFO bottom)
    WRITE_PERI_REG(SPI_USER(STM_HSPI), SPI_CK_I_EDGE | SPI_USR_MISO_HIGHPART);
    // CLOCK (reset)
    WRITE_PERI_REG(SPI_CLOCK(STM_HSPI), 0);
    SET_PERI_REG_BITS(SPI_CTRL2(STM_HSPI), SPI_MISO_DELAY_MODE, 1, SPI_MISO_DELAY_MODE_S);

    // USER1 (8-bit address phase)
    WRITE_PERI_REG(SPI_USER1(STM_HSPI), 7 << SPI_USR_ADDR_BITLEN_S);
    // USER2 (8-bit command phase)
    WRITE_PERI_REG(SPI_USER2(STM_HSPI), 7 << SPI_USR_COMMAND_BITLEN_S);
    // SLAVE1 (slave buffer size 32 byte)
    WRITE_PERI_REG(SPI_SLAVE1(STM_HSPI), ((32 * 8 - 1) << SPI_SLV_BUF_BITLEN_S));
    // PIN (CPOL low, CS)
    WRITE_PERI_REG(SPI_PIN(STM_HSPI), BIT19);

    // Отладочный вывод
    STM_SPI_DEBUG();

    // Настройка прерывание HSPI
    _xt_isr_attach(ETS_SPI_INUM, stm_link_t::spi_isr, NULL);
    _xt_isr_unmask(1 << ETS_SPI_INUM);

    // Запуск задачи обработки пакетов
    stm_task.start();
}

void stm_wait_idle(void)
{
    stm_link.event_spi_idle.wait();
}
