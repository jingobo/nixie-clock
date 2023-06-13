#ifndef __TYPEDEFS_H
#define __TYPEDEFS_H

// Общие
#include <common.h>
// IAR
#include <intrinsics.h>

// Установка опции компилятора через макрос
#define PRAGMA_OPTION(m, v)      PRAGMA(m = v)

// Включение/Отключение предупреждений
#define WARNING_SUPPRESS(err)   PRAGMA_OPTION(diag_suppress, err)
#define WARNING_DEFAULT(err)    PRAGMA_OPTION(diag_default, err)

// Секции линковщика
#define SECTION_USED(name)      PRAGMA_OPTION(location, name)
#define SECTION_DECL(name)      PRAGMA_OPTION(section, name)

// Сохраняет/Возвращает текущее состояние прерываний
#define IRQ_CTX_SAVE()          __istate_t __istate = __get_interrupt_state()
#define IRQ_CTX_RESTORE()       __set_interrupt_state(__istate)

// Включает/Отключает все маскируемеые прерывания
#define IRQ_CTX_ENABLE()        __enable_interrupt()
#define IRQ_CTX_DISABLE()       __disable_interrupt()

// Вход/Выход в безопасный  от прерываний код
#define IRQ_SAFE_LEAVE()        IRQ_CTX_RESTORE()
#define IRQ_SAFE_ENTER()        \
    IRQ_CTX_SAVE();             \
    IRQ_CTX_DISABLE()

// Модификатор для прерываний
#define IRQ_ROUTINE             PRAGMA_OPTION(optimize, speed)
        
#endif // __TYPEDEFS_H
