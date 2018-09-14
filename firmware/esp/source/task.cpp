#include "task.h"

// Имя модуля для логирования
#define MODULE_NAME     "TASK"

// Оболочка для метода выполнения задачи
ROM void task_wrapper_t::execute_wrapper(void)
{
    // Выполнение
    execute();
    // Снятие флага выполнения
    MUTEX_ENTER(mutex);
        active = false;
    MUTEX_LEAVE(mutex);
    log_module(MODULE_NAME, "Stopped \"%s\"...", name);
}

task_wrapper_t::task_wrapper_t(const char * _name)
    : mutex(MUTEX_CREATE()), active(false), name(_name)
{
    assert(name != NULL);
    assert(mutex != NULL);
}

ROM bool task_wrapper_t::start(void)
{
    auto result = false;
    MUTEX_ENTER(mutex);
        // Проверка активности задачи
        if (!active)
            if (CREATE_TASK(task_routine, name, this) != pdPASS)
                log_module(MODULE_NAME, "Create \"%s\" failed!", name);
            else
            {
                log_module(MODULE_NAME, "Started \"%s\"...", name);
                result = active = true;
            }
            log_heap(MODULE_NAME);
    MUTEX_LEAVE(mutex);
    return result;
}
