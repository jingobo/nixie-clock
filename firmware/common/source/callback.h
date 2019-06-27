#ifndef __CALLBACK_H
#define __CALLBACK_H

#include "list.h"

// Прототип функции обратного вызова
typedef void (* callback_proc_ptr)(void);
// Прототип функции обратного вызова с аргументом
typedef void (* callback_proc_arg_ptr)(void *arg);

// Класс функции обратного вызова с аргументом
class callback_t
{
    // Указатель на вызываемую функцию
    void *_proc;
    // Передаваемый аргумент
    void *_arg;
public:
    // Конструктор по умолчанию
    callback_t(void) : _proc(NULL), _arg(NULL)
    { }

    // Конструктор функции без аргумента
    callback_t(callback_proc_ptr proc) : _proc((void *)proc), _arg(NULL)
    {
        assert(_proc != NULL);
    }
    
    // Конструктор функции с аргументом
    callback_t(callback_proc_arg_ptr proc, void *arg) : _proc((void *)proc), _arg(arg)
    {
        assert(_arg != NULL);
        assert(_proc != NULL);
    }
    
    // Сравнение
    bool operator == (const callback_t &a) const
    {
        return _proc == a._proc;
    }
    
    // Присвоение
    callback_t& operator = (const callback_t& a)
    {
        _proc = a._proc;
        _arg = a._arg;
        return *this;
    }
    
    // Вызов
    void operator ()(void) const
    {
        // Проверка состояния
        assert(_proc != NULL);
        // Вызов фукции
        if (_arg != NULL)
            ((callback_proc_arg_ptr)_proc)(_arg);
        else
            ((callback_proc_ptr)_proc)();
    }
};

// Класс элемента списка функций обратного вызова
class callback_list_item_t : public list_item_t
{
    friend class callback_list_t;
    // Функция обратного вызова
    callback_t callback;
public:
    // Конструктор по умолчанию
    callback_list_item_t(callback_t cb) : callback(cb)
    { }

    // Конструктор для лямбд
    callback_list_item_t(callback_proc_ptr lambda) : callback(lambda)
    { }
};

// Класс списка функций обратного вызова
class callback_list_t : public list_template_t<callback_list_item_t>
{
public:
    // Вызов цепочки функций обратного вызова
    void operator ()(void) const
    {
        for (auto item = head(); item != NULL; item = LIST_ITEM_NEXT(item))
            item->callback();
    }
};

#endif // __CALLBACK_H
