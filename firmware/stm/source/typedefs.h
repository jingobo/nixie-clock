#ifndef __TYPEDEFS_H
#define __TYPEDEFS_H

// Общие
#include <common.h>
// IAR
#include <intrinsics.h>

// Переход в спящий режим до прерывания
#define WFI()                   __WFI()
// Установка опции компилятора через макрос
#define PRAGMA_OPTION(m, v)      PRAGMA(m = v)

// Режимы оптимизаций функций
#define OPTIMIZE_NONE           PRAGMA_OPTION(optimize, none)
#define OPTIMIZE_SIZE           PRAGMA_OPTION(optimize, size)
#define OPTIMIZE_SPEED          PRAGMA_OPTION(optimize, speed)
// Подставление функций
#define INLINE_NEVER            PRAGMA_OPTION(inline, never)
#define INLINE_FORCED           PRAGMA_OPTION(inline, forced)

// Включение/Отключение предупреждений
#define WARNING_SUPPRESS(err)   PRAGMA_OPTION(diag_suppress, err)
#define WARNING_DEFAULT(err)    PRAGMA_OPTION(diag_default, err)

// Верванивание данных
#define ALIGN_DATA_8            PRAGMA_OPTION(data_alignment, 1)
#define ALIGN_DATA_16           PRAGMA_OPTION(data_alignment, 2)
#define ALIGN_DATA_32           PRAGMA_OPTION(data_alignment, 4)

// Секции линковщика
#define SECTION_USED(name)      PRAGMA_OPTION(location, name)
#define SECTION_DECLARE(name)   PRAGMA_OPTION(section, name)

// Модификатор для прерываний
#define IRQ_ROUTINE             INLINE_NEVER OPTIMIZE_SPEED
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

#endif // __TYPEDEFS_H
