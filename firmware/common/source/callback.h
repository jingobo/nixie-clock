#ifndef __CALLBACK_H
#define __CALLBACK_H

#include "list.h"

// Прототип функции обратного вызова
typedef void (* callback_proc_ptr)(void);

// Класс элемента списка функций обратного вызова
class callback_list_item_t : public list_item_t
{
    friend class callback_list_t;
    // Функция обратного вызова
    callback_proc_ptr callback;
    
public:
    // Конструктор по умолчанию
    callback_list_item_t(callback_proc_ptr cb) : callback(cb)
    {
        assert(cb != NULL);
    }
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
