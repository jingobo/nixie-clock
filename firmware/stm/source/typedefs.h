#ifndef __TYPEDEFS_H
#define __TYPEDEFS_H

// STD
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
// IAR
#include <intrinsics.h>

// Пустой макрос
#define MACRO_EMPTY

// Пустой указатель
#ifndef NULL
    #define NULL                0
#endif

// Умножение на 1000
#define MUL_K(v)                (1000ul * (v)) 
// Умножение на 1000000
#define MUL_M(v)                (1000000ul * (v))
// Перевод милисекунд в микросекунды
#define MS_TO_US(v)             MUL_K(v)

// Создание битовых масок
#define MASK(type, value, pos)  ((type)(value) << (pos))
#define MASK_32(value, pos)     MASK(uint32_t, value, pos)

// Символ линкера в C-style
#ifdef __cplusplus
    #define EXTERN_C            extern "C"
#else
    #define EXTERN_C            MACRO_EMPTY
#endif

#define SAFE_BLOCK(code)        \
    do {                        \
        code;                   \
    } while (false)

// Вещественные типы данных
typedef float float32_t;
typedef double float64_t;
// Используемый вещественный тип по умолчанию
typedef float32_t float_t;

// Класс-заглушка
class object_dummy_t
{ };

#define PRAGMA(msg)             _Pragma(_STRINGIFY_B(msg))
#define PRAGMA_OPTION(m, v)      PRAGMA(m = v)

// Метка неиспользуемых аргументов
#define UNUSED(x)   (void)(x)
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

// Модификатор для прерываний
#define IRQ_HANDLER             INLINE_NEVER OPTIMIZE_SPEED

// Выравнивание полей в структурах
#define FIELD_ALIGN_ONE         PRAGMA(pack(1))
#define FIELD_ALIGN_TWO         PRAGMA(pack(2))
#define FIELD_ALIGN_HOUR        PRAGMA(pack(4))
        
// Верванивание данных
#define DATA_ALIGN_TWO          PRAGMA_OPTION(data_alignment, 2)
#define DATA_ALIGN_HOUR         PRAGMA_OPTION(data_alignment, 4)

// Ожидание FALSE
#define WAITWHILE(expr)         do { } while (expr)
// Ожидание TRUE
#define WAITFOR(expr)           WAITWHILE(!(expr))

// Отчистка памяти
#define MEMORY_CLEAR(var)       memset(&(var), 0, sizeof(var))
// Отчистка памяти по указателю
#define MEMORY_CLEAR_PTR(var)   memset(var, 0, sizeof(*(var)))

// Сохраняет текущее состояние прерываний
#define INTERRUPTS_SAVE()       __istate_t __isate = __get_interrupt_state()
// Возвращает сохраненное состояние прерываний
#define INTERRUPTS_RESTORE()    __set_interrupt_state(__isate)
// Отключает все маскируемеые прерывания
#define INTERRUPTS_DISABLE()    __disable_interrupt()

// Вход/Выход в безопасный  от прерываний код
#define INTERRUPT_SAFE_ENTER()  \
    INTERRUPTS_SAVE();          \
    INTERRUPTS_DISABLE()
#define INTERRUPT_SAFE_LEAVE()  INTERRUPTS_RESTORE()

// Переход в спящий режим до прерывания
#define WFI()                   __WFI()
        
// Прототип процедуры оповещения
typedef void (* notify_proc_ptr)(void);

// Прототип метода оповещения
typedef void (object_dummy_t::* notify_method_ptr)(void);

#endif // __TYPEDEFS_H
