#ifndef __OS_H
#define __OS_H

#include "system.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <freertos/event_groups.h>

// Перевод мС в тики FreeRTOS
#define OS_MS_TO_TICKS(ms)          ((ms) / portTICK_PERIOD_MS)
// Проверка хэндла на валидность
#define OS_CHECK_HANDLE(handle)     ((handle) != NULL)

// Класс обёртка для мютекса
class os_mutex_t
{
    // Мьютекс для синхронизации
    xSemaphoreHandle mutex;
public:
    // Конструктор по умолчанию
    os_mutex_t(void);
    // Деструктор
    ~os_mutex_t(void);

    // Вход
    void enter(void) const;
    void enter_irq(void) const;

    // Выход
    void leave(void) const;
    void leave_irq(void) const;
};

// Базовый класс событи¤
class os_event_base_t
{
protected:
    // Очередь используемая для имитации события
    const xQueueHandle queue;
    // Элемент очереди
    typedef uint8_t queue_item_t;
public:
    // Конструктор по умолчанию
    os_event_base_t(void);
    // Деструктор
    ~os_event_base_t(void);

    // Установка события
    void set(void);
    void set_isr(void);

    // Ожидание события
    virtual bool wait(uint32_t mills = portMAX_DELAY) = 0;
};

// Событие с автосбросом
class os_event_auto_t : public os_event_base_t
{
public:
    // Ожидание события
    virtual bool wait(uint32_t mills = portMAX_DELAY);
};

// Событие с ручным сбросом
class os_event_manual_t : public os_event_base_t
{
public:
    // Ожидание события
    virtual bool wait(uint32_t mills = portMAX_DELAY);

    // Сброс события
    void reset(void);
    void reset_isr(void);
};

// Приоритеты задач
enum os_task_priority_t
{
    OS_TASK_PRIORITY_IDLE = 0,
    OS_TASK_PRIORITY_NORMAL = 1,
    OS_TASK_PRIORITY_CRITICAL = 4,
};

// Оболочка для задач FreeRTOS
class os_task_base_t
{
    // Флаг активности задачи
    bool active;
    // Флаг перезапуска задачи
    bool restart;
    // Имя задачи
    const char * const name;

    // Оболочка для метода выполнения задачи
    void execute_wrapper(void);
    // Низкоуровневый обработчик задачи
    static void task_routine(void *arg);
protected:
    // Конструктор по умолчанию
    os_task_base_t(const char * _name);

    // Обработчик задачи
    virtual void execute(void) = 0;
public:
    // Мьютекс для синхронизации
    const os_mutex_t mutex;

    // Запуск задачи
    bool start(bool auto_restart = false);

    // Получает, выполняется ли задача
    bool running(void) const
    {
        return active;
    }

    // Устанока приоритета текущей выполняемой задачи
    static void priority_set(os_task_priority_t priority);
};

#endif // __OS_H
