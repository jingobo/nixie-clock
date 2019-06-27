#ifndef __CORE_H
#define __CORE_H

#include "os.h"
#include <ipc.h>
#include "system.h"
#include <callback.h>

// Сторона обработки (порядок не менять)
enum core_link_side_t
{
    CORE_LINK_SIDE_ESP = 0,
    CORE_LINK_SIDE_STM,
    CORE_LINK_SIDE_WEB,

    // Общее количество
    CORE_LINK_SIDE_COUNT,
};

// Класс процессора исходящих пакетов
class core_processor_out_t
{
    // Внетренний класс процессора исходящих пакетов
    class side_t : public ipc_processor_t
    {
        // Обрабатывающая сторона
        const core_link_side_t side;
    public:
        // Конструктор по умолчанию
        side_t(core_link_side_t _side) : side(_side)
        { }

        // Захват/освобождение синхронизации обработчика пакетов
        void packet_process_sync_accuire(bool get);
        // Обработка пакета
        virtual ipc_processor_status_t packet_process(const ipc_packet_t &packet, const ipc_processor_args_t &args);
        // Разбитие данных на пакеты
        virtual ipc_processor_status_t packet_split(ipc_opcode_t opcode, ipc_dir_t dir, const void *source, size_t size);
    };
public:
    side_t esp;
    side_t stm;
    side_t web;
    // Конструктор по умолчанию
    core_processor_out_t(void) : esp(CORE_LINK_SIDE_ESP), stm(CORE_LINK_SIDE_STM), web(CORE_LINK_SIDE_WEB)
    { }
};

// Класс основной задачи ядра
class core_main_task_t : public os_task_base_t
{
    // Очередь для для отложенных вызовов
    const xQueueHandle deffered_call_queue;
protected:
    // Обработчик задачи
    virtual void execute(void);
public:
    // Конструктор по умолчанию
    core_main_task_t(void);

    // Отложенный вызов
    void deffered_call(callback_t callback);

    // Отложенный вызов (для лямбд)
    void deffered_call(callback_proc_ptr lambda)
    {
        deffered_call(callback_t(lambda));
    }
};

// Основная задача ядра
extern core_main_task_t core_main_task;
// Процессор исходящих пакетов
extern core_processor_out_t core_processor_out;

// Добавление обработчика команды в хост
void core_add_command_handler(ipc_command_handler_t &handler);

#endif // __CORE_H
