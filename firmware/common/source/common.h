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

// Оборачивание кода в блок
#define CODE_BLOCK(code)        \
    do {                        \
        code;                   \
    } while (false)

// Умножение на 1000 TODO: удалить
#define XK(v)       (1000l * (v))
// Умножение на 1000000 TODO: удалить
#define XM(v)       (1000000l * (v))

// Не используемый символ TODO: удалить
#define UNUSED(x)       ((void)(x))
// Преобразование в строку значения макроса
#define STRINGIFY(x)    #x
// Опция компилятора как макрос
#define PRAGMA(msg)     _Pragma(STRINGIFY(msg))

// Выравнивание полей
#define ALIGN_FIELD_8       PRAGMA(pack(1))
#define ALIGN_FIELD_DEF     PRAGMA(pack())

// Ожидание FALSE
#define WAIT_WHILE(expr)        do { } while (expr)
// Ожидание TRUE
#define WAIT_FOR(expr)          WAIT_WHILE(!(expr))
        
// Построение битовых масок
#define MASK(type, value, pos)  ((type)(value) << (pos))
#define MASK_8(value, pos)      MASK(uint8_t, value, pos)
#define MASK_16(value, pos)     MASK(uint16_t, value, pos)
#define MASK_32(value, pos)     MASK(uint32_t, value, pos)

// Расположение кода/констант
#ifdef __GNUC__
    // ESP8266 IDF (GCC)
    #define RAM_GCC     __attribute__((section(".iram1")))
    #define RAM_IAR
#else
    // IAR ARM
    #define RAM_GCC
    #define RAM_IAR     __ramfunc
#endif

// Утверждение на этапе компиляции
#ifdef __GNUC__
    #define STATIC_ASSERT(exp)      static_assert(exp, #exp)
#else
    #define STATIC_ASSERT(exp)      static_assert(exp)
#endif

// Очистка памяти
inline void memory_clear(void *buffer, size_t size)
{
    memset(buffer, 0, size);
}

// Получает количество элементов массива
template<class T, size_t N>
constexpr size_t array_length(T (&arr)[N])
{
    return N;
}

// Получает наименьший элемент
template<typename T>
constexpr T minimum(T a, T b)
{
    return a < b ? a : b;
}

// Получает наибольший элемент
template<typename T>
constexpr T maximum(T a, T b)
{
    return a > b ? a : b;
}

// Деление с округлением в большую сторону
template<typename T>
constexpr T div_ceil(T a, T b)
{
    return a / b + (((a % b) != 0) ? 1 : 0);
}

// Прототип функции оповещения
typedef void (* handler_cb_ptr)(void);

// Исключение литералов assert для вызова из ОЗУ функций
#ifdef __IAR_SYSTEMS_ICC__
    // Будем переопределять
    #undef assert
    
    #ifdef NDEBUG
        // Заглушка
        #define assert(test)    UNUSED(0)
    #else
        // Реализация
        #define assert(test)    ((test) ? UNUSED(0) : main_assert())
        
        // Обработчик утверждения
        extern handler_cb_ptr main_assert;
    #endif
#endif

#endif // __TYPEDEFS_H
