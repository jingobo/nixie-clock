#ifndef __IO_H
#define __IO_H

#include "system.h"

// ����� �������� ����
class gpio_t
{
	// ��������� ������������� ����
	uint8_t index;
	// �������� ���������
	bool inverse, state, opm;
	// ������ ��� �������
	os_timer_t blink_timer;

	// ���������� ������� �������
	static void blink_timer_cb(void *arg);
public:
	// ������������� �������� ����
	void init(uint8_t index, uint32_t mux, uint8_t func, bool inverse);
	// ��������� ��������� ��������� ����
	void state_set(bool state);
	// ��������� ��������� ������� ����
	void blink_set(bool state);
	// �������
	void flash(void);
};

// ��������� ����������
E_SYMBOL gpio_t IO_LED_YELLOW, IO_LED_GREEN;

// ������������� ������
void io_init(void);

#endif // __IO_H
