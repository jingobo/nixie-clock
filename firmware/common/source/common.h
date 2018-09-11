#ifndef __COMMON_H
#define __COMMON_H

// Стандартная библиотека
#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// Зависимые от платоформы
#include <platform.h>

// Вещественные типы данных
typedef float float32_t;
typedef double float64_t;
// Используемый вещественный тип по умолчанию
typedef float32_t float_t;

// Оборачивание кода в блок
#define SAFE_BLOCK(code)        \
    do {                        \
        code;                   \
    } while (false)

// --- Для макроопределений и опций компилятора --- //

// Пустой макрос
#define MACRO_EMPTY
// Внешний символ
#define E_SYMBOL                extern
// Символ линковщика в C-style
#define C_SYMBOL                extern "C"
// Преобразование в строку значения макроса
#define STRINGIFY(x)            #x
// Опция компилятора (прагма) как макрос
#define PRAGMA(msg)             _Pragma(STRINGIFY(msg))
// Пустой блок
#define BLOCK_EMPTY             SAFE_BLOCK(MACRO_EMPTY)

// --- Арифметические операции --- //
        
// Умножение на 1000
#define XK(v)                	(1000l * (v))
// Умножение на 1000000
#define XM(v)                	(1000000l * (v))
// Получает размер массива в элементах
#define ARRAY_SIZE(x)           (sizeof(x) / sizeof((x)[0]))
// Деление с округлением в большую сторону
#define DIV_CEIL(x, y)          ((x) / (y) + ((x) % (y) > 0 ? 1 : 0))
// Сложение для перечислений (вынужденный костыль)
#define ENUM_DX(v, dx)          ((decltype(v))(((int)(v)) + (dx)))
#define ENUM_INC(v)             ENUM_DX(v, 1)
#define ENUM_DEC(v)             ENUM_DX(v, -1)

// --- Битовые маски --- //
        
// Создание битовых масок
#define MASK(type, value, pos)  ((type)(value) << (pos))
#define MASK_8(value, pos)     	MASK(uint8_t, value, pos)
#define MASK_16(value, pos)     MASK(uint16_t, value, pos)
#define MASK_32(value, pos)     MASK(uint32_t, value, pos)

// Не используемый символ
#define UNUSED(x)           	((void)(x))
// Ожидание FALSE
#define WAIT_WHILE(expr)         do { } while (expr)
// Ожидание TRUE
#define WAIT_FOR(expr)           WAIT_WHILE(!(expr))

// Выравнивание полей в структурах
#define ALIGN_FIELD_8           PRAGMA(pack(1))
#define ALIGN_FIELD_16          PRAGMA(pack(2))
#define ALIGN_FIELD_32          PRAGMA(pack(4))
#define ALIGN_FIELD_DEF         PRAGMA(pack())

// Отчистка памяти
#define MEMORY_CLEAR(var)       memset(&(var), 0, sizeof(var))
// Отчистка памяти по указателю
#define MEMORY_CLEAR_PTR(var)   memset(var, 0, sizeof(*(var)))

// Интерфейс события
class notify_t
{
public:
    // Событие оповещения
    virtual void notify_event(void) = 0;
};

#endif // __TYPEDEFS_H
