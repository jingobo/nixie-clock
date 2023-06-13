#ifndef __EVENT_H
#define __EVENT_H

#include <list.h>
#include <callback.h>

// Класс события
class event_t : list_item_t
{
    // Указывает, что элемент добавлен и ожидает обработки
    bool pending = false;
    // Обработчик
    callback_proc_ptr callback;

    // Обработка событий
    static void process(void);
    
public:
    // Конструктор по умолчанию
    event_t(callback_proc_ptr cb) : callback(cb)
    {
        assert(cb != NULL);
    }
    
    // Генерация события
    void raise(void);
    
    // Цикл обработки событий
    static __noreturn void loop(void);
};

#endif // __EVENT_H
