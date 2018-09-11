#include "list.h"

// ���������� �������� �������
#define OPPSIDE(p)      (LIST_SIDE_NEXT - (p))

RAM size_t list_t::count(void) const
{
    if (sides[LIST_SIDE_HEAD] == NULL)
        return 0;
    return sides[LIST_SIDE_HEAD]->count(LIST_SIDE_NEXT) + 1;
}

RAM void list_t::clear(void)
{
    if (sides[LIST_SIDE_HEAD] != NULL)
        sides[LIST_SIDE_HEAD]->uncouple(LIST_SIDE_NEXT);
}

RAM bool list_t::contains(const list_item_t &item) const
{
    if (empty())
        return false;
    return head()->contains(item);
}

ROM void list_item_t::link(list_t &list, list_side_t side)
{
    // �������� ���������
    assert(unlinked());
    // ���� ���� ��������
    if (!list.empty())
    {
        link(list.sides[side], side);
        return;
    }
    // ���� ��� ���������
    for (auto i = 0; i < LIST_SIDE_COUNT; i++)
        list.sides[i] = this;
    parent = &list;
}

ROM void list_item_t::link(list_item_t &item, list_side_t side)
{
    // �������� ���������
    assert(unlinked());
    // �������
    if (item.sides[side] != NULL)
        item.sides[side]->sides[OPPSIDE(side)] = this;
    sides[side] = item.sides[side];
    sides[OPPSIDE(side)] = &item;
    item.sides[side] = this;
    // ������������ ������
    parent = item.parent;
    if (parent != NULL && sides[side] == NULL)
        parent->sides[side] = this;
}

ROM list_item_t * list_item_t::unlink(list_side_t side)
{
    // �������� ����������
    assert(!unlinked());
    // ������� ���������
    auto result = sides[side];
    // ����� ������
    for (auto i = 0; i < LIST_SIDE_COUNT; i++)
        if (sides[i] != NULL)
        {
            // �������� �� �������
            sides[i]->sides[OPPSIDE(i)] = sides[OPPSIDE(i)];
        }
        else if (parent != NULL)
        {
            // �������� �� ������������� ������
            assert(parent->sides[i] == this);
            parent->sides[i] = sides[OPPSIDE(i)];
        }
    parent = NULL;
    clear();
    // ������
    return result;
}

RAM size_t list_item_t::count(list_side_t side) const
{
    size_t result = 0;
    for (auto temp = this->sides[side]; temp != NULL; temp = temp->sides[side])
        result++;
    return result;
}

ROM list_item_t & list_item_t::ending(list_side_t side)
{
    auto current = this;
    while (current->sides[side] != NULL)
        current = current->sides[side];
    return *current;
}
    
ROM void list_item_t::uncouple(list_side_t side)
{
    for (auto temp = this; temp != NULL;)
        temp = temp->unlink(side);
}

bool list_item_t::contains(const list_item_t &item, list_side_t side) const
{
    if (this == &item)
        return true;
    for (auto temp = this->sides[side]; temp != NULL; temp = temp->sides[side])
        if (temp == &item)
            return true;
    return false;
}
