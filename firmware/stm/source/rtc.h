#ifndef __RTC_H
#define __RTC_H

#include "typedefs.h"
#include <datetime.h>

// ������������� ������
void rtc_init(void);
// ����� ������� RTC /64
void rtc_clock_output(bool enabled);
// �������� ������� ����/�����
void rtc_datetime_get(datetime_t &dest);

// ���������� ���������� ����������
void rtc_interrupt_second(void);

#endif // __RTC_H
