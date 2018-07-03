#include "list.h"

void list_item_t::insert(list_item_t *place)
{
    list_item_t *ll;
    // Проверка аргументов
    assert(next == NULL);
    // Включение
    if (place == NULL)
        return;
    ll = place->next;
    place->next = this;
    next = ll;
}

list_item_t * list_item_t::remove(list_item_t *place)
{
    list_item_t *ll;
    // Исключение
    ll = next;
    next = NULL;
    if (place == NULL)
        return ll;
    assert(place->next == this);
    place->next = ll;
    return ll;
}

void list_item_t::uncouple(void)
{
    list_item_t *head = this;
    // Обход списка
    do
    {
        head = head->remove();
    } while (head != NULL);
}

void list_item_t::push(list_item_t *place)
{
    // Включение
    next = place;
}

list_item_t * list_item_t::last_get(void)
{
    list_item_t *head = this;
    // Обход списка
    while (head->next != NULL)
        head = head->next;
    return head;
}

void list_item_t::last_push(list_item_t &item)
{
    // Получаем последний элемент списка
    list_item_t *head = this->last_get();
    // Вставка
    item.insert(head);
}
