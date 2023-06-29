#ifndef __NVIC_H
#define __NVIC_H

#include "system.h"

// Приоритеты прерываний, от низкого к высокому
typedef enum
{
    NVIC_IRQ_PRIORITY_0 = 15,
    NVIC_IRQ_PRIORITY_1 = 14,
    NVIC_IRQ_PRIORITY_2 = 13,
    NVIC_IRQ_PRIORITY_3 = 12,
    NVIC_IRQ_PRIORITY_4 = 11,
    NVIC_IRQ_PRIORITY_5 = 10,
    NVIC_IRQ_PRIORITY_6 = 9,
    NVIC_IRQ_PRIORITY_7 = 8,
    NVIC_IRQ_PRIORITY_8 = 7,
    NVIC_IRQ_PRIORITY_9 = 6,
    NVIC_IRQ_PRIORITY_10 = 5,
    NVIC_IRQ_PRIORITY_11 = 4,
    NVIC_IRQ_PRIORITY_12 = 3,
    NVIC_IRQ_PRIORITY_13 = 2,
    NVIC_IRQ_PRIORITY_14 = 1,
    NVIC_IRQ_PRIORITY_15 = 0
} nvic_irq_priority_t;

// Низкий приоритет
constexpr const nvic_irq_priority_t NVIC_IRQ_PRIORITY_LOWEST = NVIC_IRQ_PRIORITY_0;
// Приоритет по умолчанию
constexpr const nvic_irq_priority_t NVIC_IRQ_PRIORITY_DEFAULT = NVIC_IRQ_PRIORITY_7;
// Высокий приоритет
constexpr const nvic_irq_priority_t NVIC_IRQ_PRIORITY_HIGHEST = NVIC_IRQ_PRIORITY_15;

// Режимы групп приоритетов
enum
{
    // 0 bits pre-emption priority, 4 bits subpriority
    NVIC_PRIORITYGROUP_0 = 7,
    // 1 bits pre-emption priority, 3 bits subpriority
    NVIC_PRIORITYGROUP_1 = 6,
    // 2 bits pre-emption priority, 2 bits subpriority
    NVIC_PRIORITYGROUP_2 = 5,
    // 3 bits pre-emption priority, 1 bits subpriority
    NVIC_PRIORITYGROUP_3 = 4,
    // 4 bits pre-emption priority, 0 bits subpriority
    NVIC_PRIORITYGROUP_4 = 3
};

// Текущий используемый режим группировки прерываний
constexpr const uint32_t NVIC_PRIORITYGROUP = NVIC_PRIORITYGROUP_4;

// Установка маскирования прерываний по приоритету
inline void nvic_irq_priority_mask(nvic_irq_priority_t priority)
{
    __set_BASEPRI(priority << (8 - __NVIC_PRIO_BITS));
}

// Установка приоритета прерыванию
inline void nvic_irq_priority_set(IRQn_Type irq, nvic_irq_priority_t priority)
{
    NVIC_SetPriority(irq, NVIC_EncodePriority(NVIC_PRIORITYGROUP, priority, NULL));
}

// Включает/Отключает прерывание
inline void nvic_irq_enable(IRQn_Type irq)
{
    assert(irq >= 0);
    nvic_irq_priority_set(irq, NVIC_IRQ_PRIORITY_DEFAULT);
    NVIC_EnableIRQ(irq);
}

// Инициализация модуля
void nvic_init(void);

#endif // __NVIC_H
