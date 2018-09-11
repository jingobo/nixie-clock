#include "rtc.h"
#include "clk.h"
#include "mcu.h"
#include "esp.h"
#include "nvic.h"
#include "event.h"
#include "system.h"
#include <proto/datetime.inc>

// Класс обслуживания часов реального времени
static class rtc_t : public event_base_t
{
    // Время текущее и время синхронизации
    datetime_t time_current, time_sync;
    // Состояние синхронизации
    enum
    {
        // Синхронизированно успешно
        SYNC_STATE_SUCCESS = 0,
        // Необходима синхронизация
        SYNC_STATE_REQUEST,
        // Задержка синхронизации 1
        SYNC_STATE_NEEDED_DELAY_1,
        // Задержка синхронизации 2
        SYNC_STATE_NEEDED_DELAY_2
    } sync_state;
    // Флаг состяония отправки ответа
    bool response_needed;
    
    // Обработчик команды
    class handler_command_t : public ipc_handler_command_template_t<command_datetime_t>
    {
        // Ссылка на основной класс
        rtc_t &rtc;
    protected:
        // Оповещение о поступлении данных
        virtual void notify(ipc_dir_t dir)
        {
            // Если у нас спрашивают время
            if (dir == IPC_DIR_REQUEST)
            {
                rtc.response_send();
                return;
            }
            // Если нам ответили
            switch (data.response.quality)
            {
                case command_datetime_response_t::QUALITY_SUCCESS:
                    // Получили дату TODO: применение GMT
                    rtc.sync_state = SYNC_STATE_SUCCESS;
                    rtc.time_sync = rtc.time_current = data.response.datetime;
                    break;
                case command_datetime_response_t::QUALITY_NETWORK:
                    // Ошибка сети
                    rtc.sync_state = SYNC_STATE_NEEDED_DELAY_2;
                    break;
                case command_datetime_response_t::QUALITY_NOLIST:
                    // TODO: отправка списка
                    break;
            }
        }
    public:
        // Конструктор по умолчанию
        handler_command_t(rtc_t &parent) : rtc(parent)
        { }
    } handler_command;
    
    // Обработчик простоя
    class handler_idle_t : public ipc_handler_idle_t
    {
        // Ссылка на основной класс
        rtc_t &rtc;
    protected:
        // Событие оповещения
        virtual void notify_event(void)
        {
            // Если нужно отправить ответ
            if (rtc.response_needed)
            {
                rtc.response_send();
                return;
            }
            // Если нужно запросить дату/время
            if (rtc.sync_state == SYNC_STATE_REQUEST && esp_transmit(IPC_DIR_REQUEST, rtc.handler_command.data))
                rtc.sync_state = SYNC_STATE_NEEDED_DELAY_1;
        }
    public:
        // Конструктор по умолчанию
        handler_idle_t(rtc_t &parent) : rtc(parent)
        { }
        
    } handler_idle;
    
    // Отправка ответа
    void response_send(void)
    {
        auto &response = handler_command.data.response;
        // Заполняем ответ (quality не меняем)
        datetime_get(response.datetime);
        // Отправляем ответ
        response_needed = !esp_transmit(IPC_DIR_RESPONSE, handler_command.data);
    }
protected:
    // Событие инкремента секунды
    virtual void notify_event(void)
    {
        // Задержка запроса даты/времени
        if (sync_state > SYNC_STATE_REQUEST)
            sync_state = ENUM_DEC(sync_state);
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
public:
    // Конструктор по умолчанию
    rtc_t(void) : 
        sync_state(SYNC_STATE_SUCCESS), 
        response_needed(false), 
        handler_command(*this),
        handler_idle(*this)
    { }
    
    // Инициализация
    INLINE_FORCED
    void init(void)
    {
        // Запрос даты/времени
        sync_state = SYNC_STATE_REQUEST;
        // Обработчики
        esp_handler_add_idle(handler_idle);
        esp_handler_add_command(handler_command);
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
