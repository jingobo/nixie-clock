#ifndef __LIST_H
#define __LIST_H

#include "common.h"

// ������� ������������ ��������
enum list_side_t
{
    // ���������� �������
    LIST_SIDE_PREV,
    // ��������� �������
    LIST_SIDE_NEXT,
    // ���������� ������
    LIST_SIDE_COUNT
};

// ������ �������
#define LIST_SIDE_HEAD      LIST_SIDE_PREV
// ��������� �������
#define LIST_SIDE_LAST      LIST_SIDE_NEXT

// ������� ������ (��������������� ����������)
class list_item_t;

// ������� ������
class list_sides_t
{
    friend class list_item_t;
public:
    // �������� ��������� �� ������� � ��������� �������
    RAM list_item_t * side(list_side_t side) const
    {
        return sides[side];
    }
protected:
    // ��������� �� �������� � ���������� �������
    list_item_t *sides[LIST_SIDE_COUNT];
    
    // ����������� �� ���������
    list_sides_t(void)
    {
        clear();
    }
    
    // ��������, ����� �� ���������
    RAM bool cleared(void) const
    {
        return sides[LIST_SIDE_PREV] == NULL && sides[LIST_SIDE_NEXT] == NULL;
    }
    
    // �������� ����������
    RAM void clear(void)
    {
        sides[LIST_SIDE_PREV] = sides[LIST_SIDE_NEXT] = NULL;
    }
};

// ����� ������
class list_t : public list_sides_t
{
public:
    // �������� ��������� �� ������ �������
    RAM list_item_t * head(void) const
    {
        return sides[LIST_SIDE_HEAD];
    }
    
    // �������� ��������� �� ��������� �������
    RAM list_item_t * last(void) const
    {
        return sides[LIST_SIDE_LAST];
    }
    
    // ��������, ���� �� ������
    RAM bool empty(void) const
    {
        return cleared();
    }
        
    // �������� ������
    void clear(void);

    // �������� ���������� ���������
    size_t count(void) const;
    
    // ��������, ���� �� ��������� ������� � ������
    bool contains(const list_item_t &item) const;
};

// ��������� ����� ������
template <typename ITEM>
class list_template_t : public list_t
{
public:
    // �������� ��������� �� ������ �������
    RAM ITEM * head(void) const
    {
        return (ITEM *)list_t::head();
    }
    
    // �������� ��������� �� ��������� �������
    RAM ITEM * last(void) const
    {
        return (ITEM *)list_t::last();
    }
};

// ������� ������
class list_item_t : public list_sides_t
{
    // ��������� �� ������������ ������ (�����������)
    list_t * parent;
    
    // ������ ��������
    void link_internal(list_sides_t &data, list_side_t side);
public:
    // ����������� �� ���������
    list_item_t(void) : parent(NULL)
    { }
    
    // ��������, ��������� �� �������
    RAM bool unlinked(void) const
    {
        return parent == NULL && cleared();
    }
    
    // ������ �� �������
    void link(list_t &list, list_side_t side = LIST_SIDE_NEXT);

    // ������ � ���������
    void link(list_item_t &item, list_side_t side = LIST_SIDE_NEXT);
    
    // ����������� �������� �� ������
    list_item_t * unlink(list_side_t side = LIST_SIDE_NEXT);
    
    // ������ �� �������
    RAM void link(list_t *list, list_side_t side = LIST_SIDE_NEXT)
    {
        assert(list != NULL);
        link(*list, side);
    }
    
    // ������ � ���������
    RAM void link(list_item_t *item, list_side_t side = LIST_SIDE_NEXT)
    {
        assert(item != NULL);
        link(*item, side);
    }
    
    // ������ c �������� ���������
    RAM void link_ending(list_item_t &item, list_side_t side)
    {
        link(item.ending(side), side);
    }
    
    // �������� ��������� �� ��������� �������
    RAM list_item_t * next(void) const
    {
        return sides[LIST_SIDE_NEXT];
    }
    
    // �������� ��������� �� ���������� �������
    RAM list_item_t * prev(void) const
    {
        return sides[LIST_SIDE_PREV];
    }

    // �������� ��������� �� ������������ ������
    RAM list_t * list(void)
    {
        return parent;
    }

    // ������� � ��������� �������� ��� ����������
    template <typename ITEM>
    RAM static ITEM * next(const ITEM *item)
    {
        return (ITEM *)item->next();
    }

    // ������� � ����������� �������� ��� ����������
    template <typename ITEM>
    RAM static ITEM * prev(const ITEM *item)
    {
        return (ITEM *)item->prev();
    }
    
    // �������� ���������� ��������� � ��������� �������
    size_t count(list_side_t side) const;

    // �������� �������� ������� � ��������� �������
    list_item_t & ending(list_side_t side);

    // ����������� ������ � ��������� ������� ������� � ��������
    void uncouple(list_side_t side);
    
    // ��������, ���� �� ������� � ��������� �������
    bool contains(const list_item_t &item, list_side_t side = LIST_SIDE_NEXT) const;
};

#endif // __LIST_H
