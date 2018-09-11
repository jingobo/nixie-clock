#include "log.h"

// --- Зависимые от компилятора определения --- //

// Атрибут через макрос
#define ATTRIBUTE(x)            __attribute__((x))
#define ATTRIBUTE_FN(fn, v)     ATTRIBUTE(fn(v))

// Атрибут, указывающий что у функции нет возврата
#define NO_RETURN               ATTRIBUTE(__noreturn__)

// Расположение кода/констант
#define RAM                     ATTRIBUTE_FN(section, ".text")
#define ROM                     ATTRIBUTE_FN(section, ".irom0.text")
#define NOINIT                  ATTRIBUTE_FN(section, ".noinit")
