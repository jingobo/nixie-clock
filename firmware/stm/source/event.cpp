#include "event.h"
#include "system.h"

// Списки обрабатываемых событий
static struct event_list_t
{
    // Один список накопляемый, второй обрабатываемый
    list_template_t<event_t> item[2];
    // Указатель на накопляемый список
    list_template_t<event_t> *active = item;
} event_list;

void event_t::raise(void)
{
    // Добавление в начало списка событий
    IRQ_SAFE_ENTER();
        
        // Если уже установлен - выходим
        if (pending)
        {
            IRQ_SAFE_LEAVE();
            return;
        }
        // Блокирование
        pending = true;
        this->link(*event_list.active, LIST_SIDE_HEAD);
    IRQ_SAFE_LEAVE();
}

void event_t::process(void)
{
    // Проверяем, пустой ли список
    if (event_list.active->empty())
        return;
    uint8_t i;
    IRQ_SAFE_ENTER();
        // Определяем какой список был активен
        if (event_list.active == event_list.item + 0)
            i = 0;
        else if (event_list.active == event_list.item + 1)
            i = 1;
        // Выбираем активным списком другой
        event_list.active = event_list.item + (i ^ 1);
        assert(event_list.active->empty());
    IRQ_SAFE_LEAVE();
    // Обработка очереди
    do
    {
        // Получаем первый элемент списка
        auto &event = *event_list.item[i].head();
        // Проверка состояния
        assert(event.pending);
        // Вызов события
        event.execute();
        // Удаляем из списка
        event.unlink();
        // Cбрасываем флаг ожидания
        IRQ_CTX_DISABLE();
            event.pending = false;
        IRQ_CTX_RESTORE();
    } while (!event_list.item[i].empty());
}

__noreturn void event_t::loop(void)
{
    for (;;)
        process();
}
