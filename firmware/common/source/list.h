#ifndef __LIST_H
#define __LIST_H

#include "common.h"

// Сторона относительно элемента
enum list_side_t
{
    // Предыдущий элемент
    LIST_SIDE_PREV,
    // Следущюий элемент
    LIST_SIDE_NEXT,

    // Первый элемент
    LIST_SIDE_HEAD = LIST_SIDE_PREV,
    // Последний элемент
    LIST_SIDE_LAST = LIST_SIDE_NEXT,
};

// Количество сторон
constexpr const auto LIST_SIDE_COUNT = 2;

// Получает следующий элемент
#define LIST_ITEM_NEXT(item)     ((decltype(item))(item)->next())
// Получает следующий элемент
#define LIST_ITEM_PREV(item)     ((decltype(item))(item)->prev())

// Элемент списка (предварительное объявление)
class list_item_t;

// Стороны списка
class list_sides_t
{
    friend class list_item_t;
public:
    // Получает указатель на элемент с указанной стороны
    list_item_t * side(list_side_t side) const
    {
        return sides[side];
    }
protected:
    // Указатели на следущий и предыдущий элемент
    list_item_t *sides[LIST_SIDE_COUNT];
    
    // Конструктор по умолчанию
    list_sides_t(void)
    {
        clear();
    }
    
    // Получает, пусты ли указатели
    RAM_IAR
    bool cleared(void) const
    {
        return sides[LIST_SIDE_PREV] == NULL && sides[LIST_SIDE_NEXT] == NULL;
    }
    
    // Отчистка указателей
    RAM_IAR
    void clear(void)
    {
        sides[LIST_SIDE_PREV] = sides[LIST_SIDE_NEXT] = NULL;
    }
};

// Класс списка
class list_t : public list_sides_t
{
public:
    // Получает указатель на первый элемент
    RAM_IAR
    list_item_t * head(void) const
    {
        return sides[LIST_SIDE_HEAD];
    }
    
    // Получает указатель на последний элемент
    RAM_IAR
    list_item_t * last(void) const
    {
        return sides[LIST_SIDE_LAST];
    }
    
    // Получает, пуст ли список
    RAM_IAR
    bool empty(void) const
    {
        return cleared();
    }
        
    // Отчистка списка
    void clear(void);

    // Получает количество элементов
    size_t count(void) const;
    
    // Получает, есть ли указанный элемент в списке
    bool contains(const list_item_t &item) const;
};

// Шаблонный класс списка
template <typename ITEM>
class list_template_t : public list_t
{
public:
    // Получает указатель на первый элемент
    RAM_IAR
    ITEM * head(void) const
    {
        return (ITEM *)list_t::head();
    }
    
    // Получает указатель на последний элемент
    RAM_IAR
    ITEM * last(void) const
    {
        return (ITEM *)list_t::last();
    }
};

// Элемент списка
class list_item_t : public list_sides_t
{
    // Указатель на родительский список (опционально)
    list_t *parent = NULL;
    
    // Связка элемента
    void link_internal(list_sides_t &data, list_side_t side);
public:
    // Получает, расцеплен ли элемент
    RAM_IAR
    bool unlinked(void) const
    {
        return parent == NULL && cleared();
    }

    // Получает, сцеплен ли элемент
    RAM_IAR
    bool linked(void) const
    {
        return !unlinked();
    }
    
    // Связка со списком
    RAM_IAR
    void link(list_t &list, list_side_t side = LIST_SIDE_NEXT);

    // Связка с элементом
    RAM_IAR
    void link(list_item_t &item, list_side_t side = LIST_SIDE_NEXT);
    
    // Расцепление элемента из списка
    RAM_IAR
    list_item_t * unlink(list_side_t side = LIST_SIDE_NEXT);
    
    // Связка со списком
    RAM_IAR
    void link(list_t *list, list_side_t side = LIST_SIDE_NEXT)
    {
        assert(list != NULL);
        link(*list, side);
    }
    
    // Связка с элементом
    RAM_IAR
    void link(list_item_t *item, list_side_t side = LIST_SIDE_NEXT)
    {
        assert(item != NULL);
        link(*item, side);
    }
    
    // Связка c конечным элементом
    void link_ending(list_item_t &item, list_side_t side)
    {
        link(item.ending(side), side);
    }
    
    // Получает указатель на следующий элемент
    RAM_IAR
    list_item_t * next(void) const
    {
        return sides[LIST_SIDE_NEXT];
    }
    
    // Получает указатель на предыдущий элемент
    RAM_IAR
    list_item_t * prev(void) const
    {
        return sides[LIST_SIDE_PREV];
    }

    // Получает указатель на родительский список
    list_t * list(void) const
    {
        return parent;
    }
    
    // Получает количество элементов с указанной стороны
    size_t count(list_side_t side) const;

    // Получает конечный элемент с указанной стороны
    list_item_t & ending(list_side_t side);

    // Расцепление списка в указанную сторону начиная с текущего
    void uncouple(list_side_t side);
    
    // Получает, есть ли элемент с указанной стороны
    bool contains(const list_item_t &item, list_side_t side = LIST_SIDE_NEXT) const;
};

// Класс элемента списка обработчиков события
class list_handler_item_t : public list_item_t
{
    friend class list_handler_t;
    
    // Обработчик события
    const handler_cb_ptr handler;
    
public:
    // Конструктор по умолчанию
    list_handler_item_t(handler_cb_ptr _handler) 
        : handler(_handler)
    {
        assert(handler != NULL);
    }
};

// Класс списка обработчиков события
class list_handler_t : public list_template_t<list_handler_item_t>
{
public:
    // Вызов цепочки обработчиков события
    void operator ()(void) const
    {
        for (auto item = head(); item != NULL; item = LIST_ITEM_NEXT(item))
            item->handler();
    }
};

#endif // __LIST_H
