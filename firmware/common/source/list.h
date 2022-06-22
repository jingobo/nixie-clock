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
};

// Количество сторон
constexpr const auto LIST_SIDE_COUNT = 2;

// Первый элемент
#define LIST_SIDE_HEAD      LIST_SIDE_PREV
// Последний элемент
#define LIST_SIDE_LAST      LIST_SIDE_NEXT

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
    bool cleared(void) const
    {
        return sides[LIST_SIDE_PREV] == NULL && sides[LIST_SIDE_NEXT] == NULL;
    }
    
    // Отчистка указателей
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
    list_item_t * head(void) const
    {
        return sides[LIST_SIDE_HEAD];
    }
    
    // Получает указатель на последний элемент
    list_item_t * last(void) const
    {
        return sides[LIST_SIDE_LAST];
    }
    
    // Получает, пуст ли список
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
    ITEM * head(void) const
    {
        return (ITEM *)list_t::head();
    }
    
    // Получает указатель на последний элемент
    ITEM * last(void) const
    {
        return (ITEM *)list_t::last();
    }
};

// Элемент списка
class list_item_t : public list_sides_t
{
    // Указатель на родительский список (опционально)
    list_t * parent;
    
    // Связка элемента
    void link_internal(list_sides_t &data, list_side_t side);
public:
    // Конструктор по умолчанию
    list_item_t(void) : parent(NULL)
    { }
    
    // Получает, расцеплен ли элемент
    bool unlinked(void) const
    {
        return parent == NULL && cleared();
    }

    // Получает, сцеплен ли элемент
    bool linked(void) const
    {
        return !unlinked();
    }
    
    // Связка со списком
    void link(list_t &list, list_side_t side = LIST_SIDE_NEXT);

    // Связка с элементом
    void link(list_item_t &item, list_side_t side = LIST_SIDE_NEXT);
    
    // Расцепление элемента из списка
    list_item_t * unlink(list_side_t side = LIST_SIDE_NEXT);
    
    // Связка со списком
    void link(list_t *list, list_side_t side = LIST_SIDE_NEXT)
    {
        assert(list != NULL);
        link(*list, side);
    }
    
    // Связка с элементом
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
    list_item_t * next(void) const
    {
        return sides[LIST_SIDE_NEXT];
    }
    
    // Получает указатель на предыдущий элемент
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

#endif // __LIST_H
