#ifndef __LIST_H
#define __LIST_H

#include "typedefs.h"

// Элемент связанного однонаправленного списка
class list_item_t
{
    // Ссылка на следующий элемент
    list_item_t *next;
public:
    // Конструктор по умолчанию
    list_item_t() : next(NULL) { }
    
    // Получает ссылку на следующий элемент
    INLINE_FORCED
    list_item_t * next_get(void) const
    { return next; };
    // Вставка элемента списка после place
    void insert(list_item_t *place = NULL);
    // Исключение элемента из списка после place
    list_item_t * remove(list_item_t *place = NULL);
    // Расцепление всех элементов списка, начиная с текущего
    void uncouple(void);

    // Вставка элемента списка до place
    void push(list_item_t *place = NULL);
    // Получает последний элемент списка
    list_item_t * last_get(void);
    // Вставка элемента в конец списка
    void last_push(list_item_t &item);
};

#endif // __LIST_H
