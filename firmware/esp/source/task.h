#ifndef __TASK_H
#define __TASK_H

#include "system.h"

// �������� ��� ����� FreeRTOS
class task_wrapper_t
{
    // ���� ���������� ������
    bool active;
    // ��� ������
    const char * const name;

    // �������� ��� ������ ���������� ������
    void execute_wrapper(void);
    // �������������� ���������� ������
    RAM static void task_routine(void *arg)
    {
        ((task_wrapper_t *)arg)->execute_wrapper();
        vTaskDelete(NULL);
    }
protected:
    // ����������� �� ���������
    task_wrapper_t(const char * _name);

    // ���������� ������
    virtual void execute(void) = 0;
public:
    // ������� ��� �������������
    const xSemaphoreHandle mutex;

    // ������ ������
    bool start(void);
    // ��������, ����������� �� ������
    ROM bool running(void) const
    {
        return active;
    }
};

#endif // __TASK_H
