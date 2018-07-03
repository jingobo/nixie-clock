#ifndef __DELEGATE_H
#define __DELEGATE_H

#include "typedefs.h"

/* Класс простого делегата без аругментов.
 * Поддерживается как вызов статической функции,
 * так и метод класса */
class delegate_t
{
    // Указатель на объект, может быть NULL
    void * context;
    // Указатель на вызываемый метод или функцию
    union
    {
        notify_proc_ptr proc;
        notify_method_ptr method;
    };
public:
    // Конструктор по умолчанию
    delegate_t(void) : context(NULL), method(NULL)
    { }
    // Конструктор для процедуры
    delegate_t(notify_proc_ptr routine) : context(NULL), proc(routine)
    {
        assert(routine != NULL);
    }
    // Конструктор для метода
    delegate_t(notify_method_ptr routine, void *ctx) : context(ctx), method(routine)
    {
        assert(ctx != NULL);
        assert(routine != NULL);
    }

    // Сравнение
    bool operator == (const delegate_t& a) const
    { 
        return context == a.context && 
               method == a.method;
    }
    // Присвоение
    delegate_t& operator = (const delegate_t& a)
    {
        context = a.context;
        method = a.method;
        return *this;
    }
    // Вызов
    void operator ()(void) const
    {
        // Проверка аргументов
        assert(ready());
        // Если вызов имеет контекст
        if (context != NULL)
            (((object_dummy_t *)(context))->*method)();
        else
            proc();
    }
    // Получает, пуст ли делегат
    bool ready(void) const
    {
        return method != NULL; 
    }
};

// --- Макросы инициализации делегата --- //

// Как процедура
#define DELEGATE_PROC(proc)                 delegate_t((notify_proc_ptr)(proc))
// Как метод с явным указанием класса
#define DELEGATE_METHOD(ref, type, method)  delegate_t((notify_method_ptr)&type::method, (void *)(ref))
// Как метод с указанием объекта
#define DELEGATE_METHOD_VAR(var, method)    DELEGATE_METHOD(&(var), decltype(var), method)
// Как метод с указанием объекта по указателю
#define DELEGATE_METHOD_PTR(ref, method)    DELEGATE_METHOD_VAR(*(ref), method)

#endif // __DELEGATE_H
