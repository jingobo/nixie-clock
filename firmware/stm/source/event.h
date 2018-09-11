#ifndef __EVENT_H
#define __EVENT_H

#include <list.h>

// Класс базового события
class event_base_t : public list_item_t, public notify_t
{
    friend class event_t;
    // Указывает, что элемент добавлен и ожидает обработки
    bool pending;
public:
    // Конструктор по умолчанию
    event_base_t(void) : pending(false)
    { }
protected:
    // Установка флага ожидания обработки
    virtual bool pending_set(void)
    {
        // Если уже установлен - выходим
        if (pending)
            return false;
        // Блокирование
        pending = true;
        return true;
    }

    // Сброс ожидания обработки
    virtual void pending_reset(void)
    {
        pending = false;
    }
    
    // Вызов события
    virtual void notify(void)
    {
        // Проверка состояния
        assert(pending);
        // Вызов события
        notify_event();
    }
};

// Инициализация модуля (аппаратная часть)
void event_init(void);
// Обработчка очереди событий
void event_execute(void);
// Добавление события в очередь
void event_add(event_base_t &ev);
void event_add(event_base_t *ev);

// Флаги для создания таймера
typedef uint8_t event_timer_flag_t;
#define EVENT_TIMER_FLAG_DECLARE(n)     MASK(event_timer_flag_t, 1, n)
#define EVENT_TIMER_FLAG_NONE           ((event_timer_flag_t)0)     // Маска флагов пуста
#define EVENT_TIMER_FLAG_LOOP           EVENT_TIMER_FLAG_DECLARE(0) // Таймер периодичный
#define EVENT_TIMER_FLAG_HEAD           EVENT_TIMER_FLAG_DECLARE(1) // Обрабатываются в первую очередь
#define EVENT_TIMER_FLAG_CIRQ           EVENT_TIMER_FLAG_DECLARE(2) // Вызывается из прерывания

// Приоритеты обработки таймера
#define EVENT_TIMER_PRI_DEFAULT         EVENT_TIMER_FLAG_NONE
#define EVENT_TIMER_PRI_HIGHEST         EVENT_TIMER_FLAG_HEAD
#define EVENT_TIMER_PRI_CRITICAL        (EVENT_TIMER_FLAG_HEAD | EVENT_TIMER_FLAG_CIRQ)

// Тип данных для хранения интервала в тиках
typedef uint32_t event_interval_t;

// Старт программного таймера
void event_timer_start_hz(notify_t &handler, float_t hz, event_timer_flag_t flags = EVENT_TIMER_FLAG_NONE);
void event_timer_start_us(notify_t &handler, event_interval_t us, event_timer_flag_t flags = EVENT_TIMER_FLAG_NONE);
// Стоп программного таймера (возвращает - нашелся ли таймер)
bool event_timer_stop(const notify_t &handler);

// Обработчик прерывания аппаратного таймера
void event_interrupt_timer(void);

#endif // __EVENT_H
