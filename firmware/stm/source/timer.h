#ifndef __TIMER_H
#define __TIMER_H

#include <list.h>
#include <callback.h>

#include "event.h"

// Количество микросекунд на тик
#define TIMER_US_PER_TICK       4
// Аппаратная частота таймера событий
#define TIMER_FREQUENCY_HZ      (XM(1) / TIMER_US_PER_TICK)

// Предел интервала в [мкС]
#define TIMER_US_MIN            TIMER_US_PER_TICK
#define TIMER_US_MAX            UINT32_MAX
// Предел частоты в [Гц]
#define TIMER_HZ_MIN            1
#define TIMER_HZ_MAX            TIMER_FREQUENCY_HZ

// Флаги для создания таймера
typedef uint8_t timer_flag_t;

// Декларирование маски
#define TIMER_FLAG_DECLARE(n)   MASK(timer_flag_t, 1, n)
// Маска флагов пуста
#define TIMER_FLAG_NONE         ((timer_flag_t)0)
// Таймер периодичный
#define TIMER_FLAG_LOOP         TIMER_FLAG_DECLARE(0)
// Обрабатываются в первую очередь
#define TIMER_FLAG_HEAD         TIMER_FLAG_DECLARE(1)
// Вызывается из прерывания
#define TIMER_FLAG_CIRQ         TIMER_FLAG_DECLARE(2)

// Приоритет обработки по умолчанию
#define TIMER_PRI_DEFAULT       TIMER_FLAG_NONE
// Приоритет обработки наивысший
#define TIMER_PRI_HIGHEST       TIMER_FLAG_HEAD
// Приоритет обработки из прерывания
#define TIMER_PRI_CRITICAL      (TIMER_FLAG_HEAD | TIMER_FLAG_CIRQ)

// Предварительное объявление
class timer_t;

// Списочнаяя оболочка для таймера
class timer_wrap_t : public list_item_t
{
public:
    // Ссылка на родительский таймер
    timer_t &timer;
    
    // Конструктор по умолчанию
    timer_wrap_t(timer_t &_timer) : timer(_timer)
    { }
};

// Класс софтового таймера
class timer_t
{
    // Обработчик
    callback_proc_ptr callback;
    // Общий список
    timer_wrap_t active = *this;
    // Для срабатывания
    timer_wrap_t raised = *this;
    // Счетчики интервалов
    uint32_t current, reload;
    // Вызов из прекрывания
    bool call_from_irq;
    // Событие вызова обработчиков сработавших таймеров
    static event_t call_event;
    
    // Вызов обработчиков таймеров из главного цикла
    static void call_event_cb(void);
    // Старт таймера на указанное количество тиков
    void start(uint32_t interval, timer_flag_t flags);
    
public:
    // Конструктор по умолчанию
    timer_t(callback_proc_ptr cb) : callback(cb)
    {
        assert(cb != NULL);
    }

    // Старт таймера
    void start_hz(float_t hz, timer_flag_t flags = TIMER_FLAG_NONE);
    void start_us(uint32_t us, timer_flag_t flags = TIMER_FLAG_NONE);
    // Стоп таймера
    bool stop(void);
    // Вынужденное срабатывание
    void raise(void);
    
    // Обработка таймеров в прерывании
    static void interrupt_htim(void);
};

// Инициализация модуля
void timer_init(void);

#endif // __TIMER_H
