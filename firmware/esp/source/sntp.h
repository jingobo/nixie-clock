#ifndef __SNTP_H
#define __SNTP_H

#include <datetime.h>

// ����� ������
enum sntp_mode_t
{
    // ��������������
    SNTP_MODE_RESERVED = 0,
    // ������������ ��������
    SNTP_MODE_ACTIVE,
    // ������������ ���������
    SNTP_MODE_PASSIVE,
    // ������
    SNTP_MODE_CLIENT,
    // ������
    SNTP_MODE_SERVER,
    // �����������������
    SNTP_MODE_BROADCAST,
    // �������������� ��� ����������� ��������� NTP
    SNTP_MODE_RESERVED_CONTROL,
    // �������������� ��� �������� �������������
    SNTP_MODE_RESERVED_PRIVATE,
};

// ������ ���������
enum sntp_status_t
{
    // ��� ���������
    SNTP_STATUS_NONE = 0,
    // ��������� ������ ����� ����� 61 �������
    SNTP_STATUS_PLUS,
    // ��������� ������ ����� ����� 59 ������
    SNTP_STATUS_MINUS,
    // ����� �� ����������������
    SNTP_STATUS_ALARM,
};

ALIGN_FIELD_8
// ����� � ������� SNTP
struct sntp_time_t
{
    // ����� ����� ������
    uint32_t seconds;
    // ������� ����� ������
    uint32_t fraction;

    // �������������� ���� ����� ������
    void ready(void);
    // ��������������� � ����������� �������������
    void datetime_get(datetime_t &dest) const;
};

// ����� ����� SNTP
struct sntp_packet_t
{
    union
    {
        // ����� �������� ���������
        uint8_t header;
        // ���� ���������
        struct
        {
            // ����� ������
            sntp_mode_t mode : 3;
            // ����� ������
            uint8_t version : 3;
            // ��������� ���������
            sntp_status_t status : 2;
        };
    };
    // ������
    uint8_t stratum;
    // �������� ������
    uint8_t poll;
    // ��������
    uint8_t precision;

    // ��������
    uint32_t delay;
    // ���������
    uint32_t dispersion;
    // ������������� ���������
    uint32_t reference_id;
    // �������
    struct
    {
        // ����� ����������
        sntp_time_t update;
        // ��������� �����
        sntp_time_t initial;
        // ����� �����
        sntp_time_t rx;
        // ����� ��������
        sntp_time_t tx;

        // �������������� ���� ����� ������
        ROM void ready(void)
        {
            update.ready();
            initial.ready();
            rx.ready();
            tx.ready();
        }
    } time;

    // ����������� �� ���������
    sntp_packet_t()
    {
        memset(this, 0, sizeof(*this));
        mode = SNTP_MODE_CLIENT;
        version = 3;
    }

    // �������������� ���� ����� ������
    void ready(void);

    // �������� �� ���������� ������
    bool response_check(void) const;
};
ALIGN_FIELD_DEF

#endif // __SNTP_H
