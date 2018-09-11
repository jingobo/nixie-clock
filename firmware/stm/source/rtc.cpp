#include "rtc.h"
#include "clk.h"
#include "mcu.h"
#include "esp.h"
#include "nvic.h"
#include "event.h"
#include "system.h"
#include <proto/datetime.inc>

// ����� ������������ ����� ��������� �������
static class rtc_t : public event_base_t
{
    // ����� ������� � ����� �������������
    datetime_t time_current, time_sync;
    // ��������� �������������
    enum
    {
        // ����������������� �������
        SYNC_STATE_SUCCESS = 0,
        // ���������� �������������
        SYNC_STATE_REQUEST,
        // �������� ������������� 1
        SYNC_STATE_NEEDED_DELAY_1,
        // �������� ������������� 2
        SYNC_STATE_NEEDED_DELAY_2
    } sync_state;
    // ���� ��������� �������� ������
    bool response_needed;
    
    // ���������� �������
    class handler_command_t : public ipc_handler_command_template_t<command_datetime_t>
    {
        // ������ �� �������� �����
        rtc_t &rtc;
    protected:
        // ���������� � ����������� ������
        virtual void notify(ipc_dir_t dir)
        {
            // ���� � ��� ���������� �����
            if (dir == IPC_DIR_REQUEST)
            {
                rtc.response_send();
                return;
            }
            // ���� ��� ��������
            switch (data.response.quality)
            {
                case command_datetime_response_t::QUALITY_SUCCESS:
                    // �������� ���� TODO: ���������� GMT
                    rtc.sync_state = SYNC_STATE_SUCCESS;
                    rtc.time_sync = rtc.time_current = data.response.datetime;
                    break;
                case command_datetime_response_t::QUALITY_NETWORK:
                    // ������ ����
                    rtc.sync_state = SYNC_STATE_NEEDED_DELAY_2;
                    break;
                case command_datetime_response_t::QUALITY_NOLIST:
                    // TODO: �������� ������
                    break;
            }
        }
    public:
        // ����������� �� ���������
        handler_command_t(rtc_t &parent) : rtc(parent)
        { }
    } handler_command;
    
    // ���������� �������
    class handler_idle_t : public ipc_handler_idle_t
    {
        // ������ �� �������� �����
        rtc_t &rtc;
    protected:
        // ������� ����������
        virtual void notify_event(void)
        {
            // ���� ����� ��������� �����
            if (rtc.response_needed)
            {
                rtc.response_send();
                return;
            }
            // ���� ����� ��������� ����/�����
            if (rtc.sync_state == SYNC_STATE_REQUEST && esp_transmit(IPC_DIR_REQUEST, rtc.handler_command.data))
                rtc.sync_state = SYNC_STATE_NEEDED_DELAY_1;
        }
    public:
        // ����������� �� ���������
        handler_idle_t(rtc_t &parent) : rtc(parent)
        { }
        
    } handler_idle;
    
    // �������� ������
    void response_send(void)
    {
        auto &response = handler_command.data.response;
        // ��������� ����� (quality �� ������)
        datetime_get(response.datetime);
        // ���������� �����
        response_needed = !esp_transmit(IPC_DIR_RESPONSE, handler_command.data);
    }
protected:
    // ������� ���������� �������
    virtual void notify_event(void)
    {
        // �������� ������� ����/�������
        if (sync_state > SYNC_STATE_REQUEST)
            sync_state = ENUM_DEC(sync_state);
        // ��������� �������
        if (++time_current.second <= DATETIME_SECOND_MAX)
            return;
        time_current.second = DATETIME_SECOND_MIN;
        // ��������� �����
        if (++time_current.minute <= DATETIME_MINUTE_MAX)
            return;
        time_current.minute = DATETIME_MINUTE_MIN;
        // ��������� �����
        if (++time_current.hour <= DATETIME_HOUR_MAX)
            return;
        time_current.hour = DATETIME_HOUR_MIN;
        // ��������� ����
        if (++time_current.day <= time_current.month_day_count())
            return;
        time_current.day = DATETIME_DAY_MIN;
        // ��������� �������
        if (++time_current.month <= DATETIME_MONTH_MAX)
            return;
        time_current.month = DATETIME_MONTH_MIN;
        // ��������� ���
        time_current.year++;
    }
public:
    // ����������� �� ���������
    rtc_t(void) : 
        sync_state(SYNC_STATE_SUCCESS), 
        response_needed(false), 
        handler_command(*this),
        handler_idle(*this)
    { }
    
    // �������������
    INLINE_FORCED
    void init(void)
    {
        // ������ ����/�������
        sync_state = SYNC_STATE_REQUEST;
        // �����������
        esp_handler_add_idle(handler_idle);
        esp_handler_add_command(handler_command);
    }
    
    // �������� ������� ����/�����
    INLINE_FORCED
    void datetime_get(datetime_t &dest) const
    {
        dest = time_current;
    }
} rtc;

// �������� ������ LSE
static bool rtc_check_lse(void)
{
    return (RCC->BDCR & RCC_BDCR_LSERDY) == RCC_BDCR_LSERDY;                    // Check LSE ready flag
}

// �������� ����� ���������� �������� ������ RTC
static bool rtc_check_rtoff(void)
{
    return (RTC->CRL & RTC_CRL_RTOFF) == RTC_CRL_RTOFF;                         // Check that RTC operation OFF
}

// ��������� ���������� �� ������� ���� RTC
INLINE_FORCED
static __noreturn void rtc_halt(void)
{
    mcu_halt(MCU_HALT_REASON_RTC);
}

// �������� ���������� ���� �������� ������ RTC
static void rtc_wait_operation_off(void)
{
    if (!clk_pool(rtc_check_rtoff))
        rtc_halt();
}

void rtc_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;                      // Power, backup interface clock enable
    BKP_ACCESS_ALLOW();
        // ���������������� LSE, RTC
        mcu_reg_update_32(&RCC->BDCR,                                           // LSE bypass off, LSE select for RTC, 
            RCC_BDCR_RTCSEL_LSE, 
            RCC_BDCR_LSEBYP | RCC_BDCR_RTCSEL);
        // ������ LSE
        RCC->BDCR |= RCC_BDCR_LSEON;                                           // LSE enable
        // �������� ������� LSE, 6 ���
        if (!clk_pool(rtc_check_lse, 6000))
            rtc_halt();
        // ������ RTC
        RCC->BDCR |= RCC_BDCR_RTCEN;                                            // RTC enable
        // ���������������� RTC
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
    // ����������
    nvic_irq_enable_set(RTC_IRQn, true);                                        // RTC IRQ enable
    nvic_irq_priority_set(RTC_IRQn, NVIC_IRQ_PRIORITY_LOWEST);                  // Lowest RTC IRQ priority
    // ��������� ����������
    rtc.init();
}

void rtc_clock_output(bool enabled)
{
    // �������������������� PC13 �� ����� � �������������� �������� ������������ ���������
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
