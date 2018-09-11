#ifndef __DATETIME_H
#define __DATETIME_H

#include "common.h"

// ������� ��� ����
#define DATETIME_YEAR_MIN       1900
#define DATETIME_YEAR_MAX       2099
// ������� ��� ������
#define DATETIME_MONTH_MIN      1
#define DATETIME_MONTH_MAX      12
// ������� ��� ����
#define DATETIME_DAY_MIN        1
#define DATETIME_DAY_MAX        31
// ������� ��� �����
#define DATETIME_HOUR_MIN       0
#define DATETIME_HOUR_MAX       23
// ������� ��� �����
#define DATETIME_MINUTE_MIN     0
#define DATETIME_MINUTE_MAX     59
// ������� ��� ������
#define DATETIME_SECOND_MIN     0
#define DATETIME_SECOND_MAX     59

ALIGN_FIELD_8
// ����������� ����/�����
struct datetime_t
{
    // ����
    uint16_t year;
    uint8_t month;
    uint8_t day;
    // �����
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    // ����������� �� ���������
    datetime_t();

    // �������� �� ����������
    bool check(void) const;
    // ��������, ���������� �� ���
    bool leap(void) const;
    
    // �������� ���������� ���� � ������
    uint8_t month_day_count(void) const;
    // �������� ���������� ���� � ������
    static uint8_t month_day_count(uint8_t month, bool leap);
};
ALIGN_FIELD_DEF

#endif // __DATETIME_H
