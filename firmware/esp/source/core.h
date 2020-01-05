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

        // Обработка пакета
        virtual bool packet_process(const ipc_packet_t &packet, const args_t &args) override final;
    };
public:
    side_t esp;
    side_t stm;
    side_t web;
    // Конструктор по умолчанию
    core_processor_out_t(void) : esp(CORE_LINK_SIDE_ESP), stm(CORE_LINK_SIDE_STM), web(CORE_LINK_SIDE_WEB)
    { }
};

// Процессор исходящих пакетов
extern core_processor_out_t core_processor_out;

// Добавление обработчика команды в хост
void core_handler_add(ipc_handler_t &handler);

#endif // __CORE_H
