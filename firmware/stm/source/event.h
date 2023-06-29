#ifndef __EVENT_H
#define __EVENT_H

#include <list.h>

// Класс события
class event_t : list_item_t
{
    // Указывает, что элемент добавлен и ожидает обработки
    bool pending = false;
    // Обработчик
    handler_cb_ptr handler;

    // Обработка событий
    static void process(void);
    
public:
    // Конструктор по умолчанию
    event_t(handler_cb_ptr _handler) : handler(_handler)
    {
        assert(handler != NULL);
    }
    
    // Генерация события
    RAM_IAR
    void raise(void);
    
    // Цикл обработки событий
    static __noreturn void loop(void);
};

#endif // __EVENT_H
