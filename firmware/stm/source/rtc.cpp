#include "rtc.h"
#include "clk.h"
#include "mcu.h"
#include "esp.h"
#include "nvic.h"
#include "event.h"
#include "system.h"
#include "storage.h"
#include <proto/datetime.inc>

// Настройки синхронизации
static datetime_sync_settings_t rtc_sync_settings @ STORAGE_SECTION =
{
    // Синхронизация включена
    .sync = IPC_BOOL_TRUE,
    // UTC +03:00
    .timezone = 6,
    // Без смещения
    .offset = 0,
    // Раз в час
    .period = datetime_sync_settings_t::SYNC_PERIOD_HOUR,
    // Список хостов по умолчанию
    .hosts = "ntp1.stratum2.ru\n0.pool.ntp.org"
};

// Класс обслуживания часов реального времени
static class rtc_t : public event_base_t, ipc_handler_event_t
{
    // Время текущее и время синхронизации
    datetime_t time_current, time_sync;
    
    // Обработчик команды синхронизации даты/времени
    class handler_command_sync_t : public ipc_handler_command_template_t<datetime_command_sync_t>
    {
        // Состояние синхронизации
        enum
        {
            // Синхронизированно успешно
            STATE_SUCCESS = 0,
            // Ожидание завершения синхронизации
            STATE_RESPONSE,
            // Необходима синхронизация
            STATE_REQUEST,
            // Задержка синхронизации 1
            STATE_NEEDED_DELAY_1,
            // Задержка синхронизации 2
            STATE_NEEDED_DELAY_2
        } state;
        // Ссылка на основной класс
        rtc_t &rtc;
    protected:
        // Оповещение о поступлении данных
        virtual void notify(ipc_dir_t dir)
        {
            // Мы можем только обрабатывать ответ
            assert(dir == IPC_DIR_RESPONSE);
            // Если ошибка, переспросим
            if (!data.response.valid)
            {
                state = STATE_NEEDED_DELAY_2;
                return;
            }
            // Получили дату TODO: применение GMT
            state = STATE_SUCCESS;
            rtc.time_sync = rtc.time_current = data.response.datetime;
        }
    public:
        // Конструктор по умолчанию
        handler_command_sync_t(rtc_t &parent) : rtc(parent), state(STATE_REQUEST)
        { }
        
        // Обработчик простоя
        void idle(void)
        {
            if (state == STATE_REQUEST && esp_transmit(IPC_DIR_REQUEST, data))
                // Ожидаем ответ
                state = STATE_RESPONSE;
        }
        
        // Обработчик сброса
        void reset(void)
        {
            // Перезапрашиваем
            if (state == STATE_RESPONSE)
                state = STATE_REQUEST;
        }
        
        // Секундное событие
        void second(void)
        {
            // Задержка запроса даты/времени
            if (state > STATE_REQUEST)
                state = ENUM_DEC(state);
        }
    } handler_command_sync;

    // Обработчик команды запроса списка SNTP хостов
    class handler_command_hosts_t : public ipc_handler_command_template_t<datetime_command_hosts_require_t>
    {
        // Ссылка на основной класс
        rtc_t &rtc;
        // Состояние
        bool sending;
        // Команда установки хостов
        datetime_command_hosts_t command;
    protected:
        // Оповещение о поступлении данных
        virtual void notify(ipc_dir_t dir)
        {
            // Мы можем только обрабатывать запрос
            assert(dir == IPC_DIR_REQUEST);
            // Заполняем ответ
            datetime_hosts_data_copy(command.request.hosts, rtc_sync_settings.hosts);
            // Указываем что нужно отправить ответ
            sending = true;
        }
    public:
        // Конструктор по умолчанию
        handler_command_hosts_t(rtc_t &parent) : rtc(parent), sending(false)
        { }
        
        // Обработчик простоя
        void idle(void)
        {
            if (sending)
                sending = !esp_transmit(IPC_DIR_REQUEST, command);
        }
    } handler_command_hosts;
protected:
    // Событие простоя
    virtual void idle(void)
    {
        // Базовый метод
        ipc_handler_event_t::idle();
        // Если нужно отправить список серверов
        handler_command_hosts.idle();
        // Если нужно запросить дату/время
        handler_command_sync.idle();
    }
    
    // Событие сброса
    virtual void reset(void)
    {
        // Базовый метод
        ipc_handler_event_t::reset();
        // Сбросы команд
        handler_command_sync.reset();
    }
public:
    // Конструктор по умолчанию
    rtc_t(void) : 
        handler_command_sync(*this),
        handler_command_hosts(*this)
    { }

    // Событие инкремента секунды
    virtual void notify(void)
    {
        handler_command_sync.second();
        // Инкремент секунды
        if (++time_current.second <= DATETIME_SECOND_MAX)
            return;
        time_current.second = DATETIME_SECOND_MIN;
        // Инкремент минут
        if (++time_current.minute <= DATETIME_MINUTE_MAX)
            return;
        time_current.minute = DATETIME_MINUTE_MIN;
        // Инкремент часов
        if (++time_current.hour <= DATETIME_HOUR_MAX)
            return;
        time_current.hour = DATETIME_HOUR_MIN;
        // Инкремент дней
        if (++time_current.day <= time_current.month_day_count())
            return;
        time_current.day = DATETIME_DAY_MIN;
        // Инкремент месяцев
        if (++time_current.month <= DATETIME_MONTH_MAX)
            return;
        time_current.month = DATETIME_MONTH_MIN;
        // Инкремент лет
        time_current.year++;
    }
    
    // Инициализация
    INLINE_FORCED
    void init(void)
    {
        esp_handler_add_event(*this);
        esp_handler_add_command(handler_command_sync);
        esp_handler_add_command(handler_command_hosts);
    }
    
    // Получает текущую дату/время
    INLINE_FORCED
    void datetime_get(datetime_t &dest) const
    {
        dest = time_current;
    }
} rtc;

// Проверка работы LSE
static bool rtc_check_lse(void)
{
    return (RCC->BDCR & RCC_BDCR_LSERDY) == RCC_BDCR_LSERDY;                    // Check LSE ready flag
}

// Проверка флага завершения операций записи RTC
static bool rtc_check_rtoff(void)
{
    return (RTC->CRL & RTC_CRL_RTOFF) == RTC_CRL_RTOFF;                         // Check that RTC operation OFF
}

// Остановка приложения по причине сбоя RTC
INLINE_FORCED
static __noreturn void rtc_halt(void)
{
    mcu_halt(MCU_HALT_REASON_RTC);
}

// Ожидание завершения всех операций записи RTC
static void rtc_wait_operation_off(void)
{
    if (!clk_pool(rtc_check_rtoff))
        rtc_halt();
}

void rtc_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;                      // Power, backup interface clock enable
    BKP_ACCESS_ALLOW();
        // Конфигурирование LSE, RTC
        mcu_reg_update_32(&RCC->BDCR,                                           // LSE bypass off, LSE select for RTC, 
            RCC_BDCR_RTCSEL_LSE, 
            RCC_BDCR_LSEBYP | RCC_BDCR_RTCSEL);
        // Запуск LSE
        RCC->BDCR |= RCC_BDCR_LSEON;                                           // LSE enable
        // Ожидание запуска LSE, 6 сек
        if (!clk_pool(rtc_check_lse, 6000))
            rtc_halt();
        // Запуск RTC
        RCC->BDCR |= RCC_BDCR_RTCEN;                                            // RTC enable
        // Конфигурирование RTC
        rtc_wait_operation_off();
            RTC->CRL |= RTC_CRL_CNF;                                            // Enter cfg mode
                RTC->PRLL = 0x7FFF;                                             // /32768
                RTC->PRLH = 0;
                RTC->CNTL = 0;                                                  // Clear counter
                RTC->CNTH = 0;
                RTC->CRH = RTC_CRH_SECIE;                                       // Second IRQ enable
            RTC->CRL &= ~RTC_CRL_CNF;                                           // Leave cfg mode
        rtc_wait_operation_off();
    BKP_ACCESS_DENY();
    // Прерывание
    nvic_irq_enable_set(RTC_IRQn, true);                                        // RTC IRQ enable
    nvic_irq_priority_set(RTC_IRQn, NVIC_IRQ_PRIORITY_LOWEST);                  // Lowest RTC IRQ priority
    // Локальные переменные
    rtc.init();
}

void rtc_clock_output(bool enabled)
{
    // Переконфигурирование PC13 на выход с альтернативной функцией производится аппаратно
    BKP_ACCESS_ALLOW();
        if (enabled)
            BKP->RTCCR |= BKP_RTCCR_CCO;                                        // Calibration clock output enable
        else
            BKP->RTCCR &= ~BKP_RTCCR_CCO;                                       // ...disable
    BKP_ACCESS_DENY();
}

void rtc_datetime_get(datetime_t &dest)
{
    rtc.datetime_get(dest);
}

IRQ_ROUTINE
void rtc_interrupt_second(void)
{
    RTC->CRL &= ~RTC_CRL_SECF;                                                  // Clear IRQ pending flag
    event_add(rtc);
}
