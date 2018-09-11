#include "event.h"

ROM bool event_auto_t::wait(uint32_t mills)
{
    queue_item_t dummy;
    return xQueueReceive(queue, &dummy, mills / portTICK_PERIOD_MS) != pdFALSE;
}

ROM bool event_manual_t::wait(uint32_t mills)
{
    queue_item_t dummy;
    return xQueuePeek(queue, &dummy, mills / portTICK_PERIOD_MS) != pdFALSE;
}
