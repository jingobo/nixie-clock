#ifndef __EVENT_H
#define __EVENT_H

#include "list.h"
#include "delegate.h"

// Тип данных для хранения интервала в тиках
typedef uint32_t event_interval_t;

// Контроллер класса события
struct event_item_controller_t;

// Класс события
class event_item_t : public list_item_t
{
    friend event_item_controller_t;
    // Указывает, что элемент добавлен и ожидает обработки
    bool pending;
    // Обработчик события
    delegate_t delegate;
public:
    // Конструктор по умолчанию
    event_item_t(delegate_t _delegate);
};

// Инициализация модуля (аппаратная часть)
void event_init(void);
// Обработчка очереди событий (возвращает - нужно ли еще раз обработать)
bool event_execute(void);
// Добавление события в очередь
void event_add(event_item_t &item);

// Флаги для создания таймера
typedef uint8_t event_timer_flag_t;
#define EVENT_TIMER_FLAG_DECLARE(n)     MASK(event_timer_flag_t, 1, n)
#define EVENT_TIMER_FLAG_NONE           ((event_timer_flag_t)0)     // Маска флагов пуста
#define EVENT_TIMER_FLAG_LOOP           EVENT_TIMER_FLAG_DECLARE(0) // Таймер периодичный
#define EVENT_TIMER_FLAG_HEAD           EVENT_TIMER_FLAG_DECLARE(1) // Обрабатываются в первую очередь
#define EVENT_TIMER_FLAG_CIRQ           EVENT_TIMER_FLAG_DECLARE(2) // Вызывается из прерывания

// Приоритеты обработки таймера
#define EVENT_TIMER_PRI_DEFAULT         (EVENT_TIMER_FLAG_NONE)
#define EVENT_TIMER_PRI_HIGHEST         (EVENT_TIMER_FLAG_HEAD)
#define EVENT_TIMER_PRI_CRITICAL        (EVENT_TIMER_FLAG_HEAD | EVENT_TIMER_FLAG_CIRQ)

// Старт программного таймера
void event_timer_start_us(const delegate_t &handler, event_interval_t us, event_timer_flag_t flags = EVENT_TIMER_FLAG_NONE);
void event_timer_start_hz(const delegate_t &handler, float_t hz, event_timer_flag_t flags = EVENT_TIMER_FLAG_NONE);
// Стоп программного таймера (возвращает - нашелся ли таймер)
bool event_timer_stop(const delegate_t handler);

// Обработчик прерывания аппаратного таймера
void event_interrupt_timer(void);

#endif // __EVENT_H
