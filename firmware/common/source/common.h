#ifndef __COMMON_H
#define __COMMON_H

// Стандартная библиотека
#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Вещественные типы данных
typedef float float32_t;
typedef double float64_t;

// Вещественный тип по умолчанию
typedef float32_t float_t;

// --- Для макроопределений --- //

// Оборачивание кода в блок
#define CODE_BLOCK(code)        \
    do {                        \
        code;                   \
    } while (false)

// Пустой макрос
#define MACRO_EMPTY
// Преобразование в строку значения макроса
#define STRINGIFY(x)            #x
// Опция компилятора как макрос
#define PRAGMA(msg)             _Pragma(STRINGIFY(msg))
// Пустой блок
#define EMPTY_BLOCK             CODE_BLOCK(MACRO_EMPTY)

// --- Зависимое от компилятора --- //

// Расположение кода/констант
#ifdef __GNUC__
    // ESP8266 IDF (GCC)
    #define RAM                 __attribute__((section(".iram1")))
#else
    // Остальные
    #define RAM                 MACRO_EMPTY
#endif
        
// --- Битовые маски --- //
        
#define MASK(type, value, pos)  ((type)(value) << (pos))
#define MASK_8(value, pos)      MASK(uint8_t, value, pos)
#define MASK_16(value, pos)     MASK(uint16_t, value, pos)
#define MASK_32(value, pos)     MASK(uint32_t, value, pos)

// --- Выравнивание полей --- //

#define ALIGN_FIELD_8           PRAGMA(pack(1))
#define ALIGN_FIELD_DEF         PRAGMA(pack())

// --- Работа с памятью --- //

// Отчистка памяти
#define MEMORY_CLEAR(var)       memset(&(var), 0, sizeof(var))
// Отчистка памяти по указателю
#define MEMORY_CLEAR_PTR(var)   memset(var, 0, sizeof(*(var)))
// Отчистка памяти для массива
#define MEMORY_CLEAR_ARR(arr)   memset(arr, 0, sizeof(arr))

// --- Арифметические операции --- //
        
// Умножение на 1000
#define XK(v)                   (1000l * (v))
// Умножение на 1000000
#define XM(v)                   (1000000l * (v))
// Получает наименьший элемент
#define MIN(a, b)               (((a) < (b)) ? (a) : (b))
// Получает размер массива в элементах
#define ARRAY_SIZE(x)           (sizeof(x) / sizeof((x)[0]))
// Деление с округлением в большую сторону
#define DIV_CEIL(x, y)          ((x) / (y) + ((x) % (y) > 0 ? 1 : 0))

// --- Разное --- //

// Не используемый символ
#define UNUSED(x)               ((void)(x))
// Отчистка строки
#define CSTR_CLEAR(x)           (x)[0] = 0
// Ожидание FALSE
#define WAIT_WHILE(expr)        do { } while (expr)
// Ожидание TRUE
#define WAIT_FOR(expr)          WAIT_WHILE(!(expr))

// Сложение для перечислений (вынужденный костыль)
#define ENUM_VALUE_SHIFT(v, dx)     ((decltype(v))(((int)(v)) + (dx)))
#define ENUM_VALUE_NEXT(v)          ENUM_VALUE_SHIFT(v, 1)
#define ENUM_VALUE_PREV(v)          ENUM_VALUE_SHIFT(v, -1)

// --- Утверждения --- //

#ifndef STATIC_ASSERT
    #ifdef __GNUC__
        #define STATIC_ASSERT(exp)  static_assert(exp, #exp)
    #else
        #define STATIC_ASSERT(exp)  static_assert(exp)
    #endif
#endif

#endif // __TYPEDEFS_H
