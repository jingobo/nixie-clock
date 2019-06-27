#include "rtc.h"
#include "mcu.h"
#include "esp.h"
#include "nvic.h"
#include "event.h"
#include "system.h"
#include "storage.h"
#include <proto/datetime.inc>

// Включает/Отключает доступ на запись в бэкап домен 
#define RTC_BKP_ACCESS_ALLOW()      PWR->CR |= PWR_CR_DBP                       // Disable backup domain write protection
#define RTC_BKP_ACCESS_DENY()       PWR->CR &= ~PWR_CR_DBP                      // Enable backup domain write protection

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

// Хранит локальные времена
static struct
{
    // ...NTP синхронизации
    datetime_t sync;
    // ...текущее
    datetime_t current;
} rtc_time;

// Обработчик команды получения даты/времени
static class rtc_command_date_get_t : public ipc_command_handler_template_t<datetime_command_get_t>
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
    } state = STATE_SUCCESS;
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать ответ
        assert(dir == IPC_DIR_RESPONSE);
        // Если ошибка, переспросим
        switch (command.response.status)
        {
            case datetime_command_get_response_t::STATUS_VALID:
                // Получили дату TODO: применение GMT
                state = STATE_SUCCESS;
                rtc_time.sync = rtc_time.current = command.response.datetime;
                break;
            case datetime_command_get_response_t::STATUS_NETWORK:
                // Сетевая ошибка, перезапрос через задержку
                state = STATE_NEEDED_DELAY_2;
                break;
            case datetime_command_get_response_t::STATUS_HOSTLIST:
                // TODO: Нет хостов, отаправляем
                state = STATE_NEEDED_DELAY_2;
                break;
            default:
                assert(false);
                break;
        }
    }
public:
    // Сброс
    void reset(void)
    {
        // Перезапрашиваем
        state = STATE_REQUEST;
    }
    
    // Простой
    void idle(void)
    {
        if (state == STATE_REQUEST && esp_transmit(IPC_DIR_REQUEST, command))
            // Ожидаем ответ
            state = STATE_RESPONSE;
    }
        
    // Секундное событие
    void second(void)
    {
        // Задержка запроса даты/времени
        if (state > STATE_REQUEST)
            state = ENUM_VALUE_PREV(state);
    }
} rtc_command_date_get;

// Обработчик команды запроса списка SNTP хостов
static class rtc_command_hosts_set_t : public ipc_command_handler_template_t<datetime_command_hosts_set_t>
{
    // Хранит, отправили ли запрос
    bool sended = false;
protected:
    // Оповещение о поступлении данных
    virtual void notify(ipc_dir_t dir)
    {
        // Мы можем только обрабатывать ответ
        assert(dir == IPC_DIR_RESPONSE);
        // Сброс
        reset();
    }
public:
    // Передача команды
    void transmit(void)
    {
        // Если уже отправили
        if (sended)
            return;
        // Заполняем ответ
        datetime_hosts_data_copy(command.request.hosts, rtc_sync_settings.hosts);
        // Передачем
        sended = esp_transmit(IPC_DIR_REQUEST, command);
    }
    
    // Сброс
    void reset(void)
    {
        sended = false;
        rtc_command_date_get.reset();
    }
} rtc_command_hosts_set;

// Класс обслуживания часов реального времени
static class rtc_ipc_handler_t : public ipc_event_handler_t
{
protected:
    // Событие простоя
    virtual void idle(void)
    {
        // Базовый метод
        ipc_event_handler_t::idle();
        // Команды
        rtc_command_date_get.idle();
    }
    
    // Событие сброса
    virtual void reset(void)
    {
        // Базовый метод
        ipc_event_handler_t::reset();
        // Команды
        rtc_command_hosts_set.reset();
    }
} rtc_ipc_handler;

// Список обработчиков секундных собюытий
static callback_list_t rtc_second_event_callbacks;

// Обработчик события инкремента секунды
static event_callback_t rtc_second_event([](void)
{
    // Оповещение о секунде
    rtc_command_date_get.second();
    // Инкремент секунды
    rtc_time.current.inc_second();
    // Вызов цепочки обработчиков
    rtc_second_event_callbacks();
});

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
__noreturn static void rtc_halt(void)
{
    mcu_halt(MCU_HALT_REASON_RTC);
}

// Ожидание завершения всех операций записи RTC
static void rtc_wait_operation_off(void)
{
    if (!mcu_pool_ms(rtc_check_rtoff))
        rtc_halt();
}

void rtc_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;                      // Power, backup interface clock enable
    IRQ_SAFE_ENTER();
        RTC_BKP_ACCESS_ALLOW();
            // Конфигурирование LSE, RTC
            mcu_reg_update_32(&RCC->BDCR,                                       // LSE bypass off, LSE select for RTC, 
                RCC_BDCR_RTCSEL_LSE, 
                RCC_BDCR_LSEBYP | RCC_BDCR_RTCSEL);
            // Запуск LSE
            RCC->BDCR |= RCC_BDCR_LSEON;                                        // LSE enable
            // Ожидание запуска LSE, 6 сек
            if (!mcu_pool_ms(rtc_check_lse, 6000))
                rtc_halt();
            // Запуск RTC
            RCC->BDCR |= RCC_BDCR_RTCEN;                                        // RTC enable
            // Конфигурирование RTC
            rtc_wait_operation_off();
                RTC->CRL |= RTC_CRL_CNF;                                        // Enter cfg mode
                    RTC->PRLL = 0x7FFF;                                         // /32768
                    RTC->PRLH = 0;
                    RTC->CNTL = 0;                                              // Clear counter
                    RTC->CNTH = 0;
                    RTC->CRH = RTC_CRH_SECIE;                                   // Second IRQ enable
                RTC->CRL &= ~RTC_CRL_CNF;                                       // Leave cfg mode
            rtc_wait_operation_off();
        RTC_BKP_ACCESS_DENY();
    IRQ_SAFE_LEAVE();
    // Прерывание
    nvic_irq_enable_set(RTC_IRQn, true);                                        // RTC IRQ enable
    nvic_irq_priority_set(RTC_IRQn, NVIC_IRQ_PRIORITY_LOWEST);                  // Lowest RTC IRQ priority
    // IPC
    //esp_add_event_handler(rtc_ipc_handler);
    //esp_add_command_handler(rtc_command_date_get);
    //esp_add_command_handler(rtc_command_hosts_set);
}

void rtc_clock_output(bool enabled)
{
    IRQ_SAFE_ENTER();
        RTC_BKP_ACCESS_ALLOW();
            // Переконфигурирование PC13 на выход с альтернативной функцией производится аппаратно
            if (enabled)
                BKP->RTCCR |= BKP_RTCCR_CCO;                                    // Calibration clock output enable
            else
                BKP->RTCCR &= ~BKP_RTCCR_CCO;                                   // ...disable
        RTC_BKP_ACCESS_DENY();
    IRQ_SAFE_LEAVE();
}

void rtc_datetime_get(datetime_t &dest)
{
    dest = rtc_time.current;
}

void rtc_second_event_add(callback_list_item_t &callback)
{
    callback.link(rtc_second_event_callbacks);
}

IRQ_ROUTINE
void rtc_interrupt_second(void)
{
    RTC->CRL &= ~RTC_CRL_SECF;                                                  // Clear IRQ pending flag
    rtc_second_event.raise();
}
