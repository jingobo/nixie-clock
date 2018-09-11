#ifndef __TASK_H
#define __TASK_H

#include "system.h"

// Оболочка для задач FreeRTOS
class task_wrapper_t
{
    // Флаг активности задачи
    bool active;
    // Имя задачи
    const char * const name;

    // Оболочка для метода выполнения задачи
    void execute_wrapper(void);
    // Низкоуровневый обработчик задачи
    RAM static void task_routine(void *arg)
    {
        ((task_wrapper_t *)arg)->execute_wrapper();
        vTaskDelete(NULL);
    }
protected:
    // Конструктор по умолчанию
    task_wrapper_t(const char * _name);

    // Обработчик задачи
    virtual void execute(void) = 0;
public:
    // Мьютекс для синхронизации
    const xSemaphoreHandle mutex;

    // Запуск задачи
    bool start(void);
    // Получает, выполняется ли задача
    ROM bool running(void) const
    {
        return active;
    }
};

#endif // __TASK_H
