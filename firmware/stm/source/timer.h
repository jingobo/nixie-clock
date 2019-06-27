﻿#ifndef __TIMER_H
#define __TIMER_H

#include <list.h>
#include <callback.h>

#include "event.h"

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
class timer_base_t;

// Списочнаяя оболочка для таймера
class timer_wrap_t : public list_item_t
{
public:
    // Ссылка на родительский таймер
    timer_base_t &timer;
    
    // Конструктор по умолчанию
    timer_wrap_t(timer_base_t &_timer) : timer(_timer)
    { }
};

// Класс базового софтового таймера
class timer_base_t
{
    // Общий список
    timer_wrap_t active;
    // Для срабатывания
    timer_wrap_t raised;
    // Счетчики интервалов
    uint32_t current, reload;
    // Вызов из прекрывания
    bool call_from_irq;
    // Событие вызова обработчиков сработавших таймеров
    static event_callback_t call_event;
    
    // Вызов обработчиков таймеров из главного цикла
    static void call_event_cb(void);
    // Старт таймера на указанное количество тиков
    void start(uint32_t interval, timer_flag_t flags);
protected:
    // Обработка тика таймера
    virtual void execute(void) = 0;
public:
    // Конструктор по умолчанию
    timer_base_t(void) : active(*this), raised(*this)
    { }

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

// Таймер с функцией обратного вызова
class timer_callback_t : public timer_base_t
{
    callback_t callback;
protected:
    // Обработка тика таймера
    virtual void execute(void)
    {
        callback();
    }
public:
    // Конструктор по умолчанию
    timer_callback_t(callback_t cb) : callback(cb)
    { }
    
    // Конструктор для лямбд
    timer_callback_t(callback_proc_ptr lambda) : callback(lambda)
    { } 
};

// Инициализация модуля
void timer_init(void);

#endif // __TIMER_H
