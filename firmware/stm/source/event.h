#ifndef __EVENT_H
#define __EVENT_H

#include <list.h>
#include <callback.h>

// Класс базового события
class event_t : list_item_t
{
    // Указывает, что элемент добавлен и ожидает обработки
    bool pending = false;
    
    // Обработка событий
    static void process(void);
protected:
    // Обработка события
    virtual void execute(void) = 0;
public:
    // Генерация события
    void raise(void);
    
    // Цикл обработки событий
    static __noreturn void loop(void);
};

// Событие с функцией обратного вызова
class event_callback_t : public event_t
{
    callback_t callback;
protected:
    // Обработка события
    virtual void execute(void) override final
    {
        callback();
    }
public:
    // Конструктор по умолчанию
    event_callback_t(callback_t cb) : callback(cb)
    { }
    
    // Конструктор для лямбд
    event_callback_t(callback_proc_ptr lambda) : callback(lambda)
    { }
};

#endif // __EVENT_H
