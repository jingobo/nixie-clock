#include "log.h"

// --- ��������� �� ����������� ����������� --- //

// ������� ����� ������
#define ATTRIBUTE(x)            __attribute__((x))
#define ATTRIBUTE_FN(fn, v)     ATTRIBUTE(fn(v))

// �������, ����������� ��� � ������� ��� ��������
#define NO_RETURN               ATTRIBUTE(__noreturn__)

// ������������ ����/��������
#define RAM                     ATTRIBUTE_FN(section, ".text")
#define ROM                     ATTRIBUTE_FN(section, ".irom0.text")
#define NOINIT                  ATTRIBUTE_FN(section, ".noinit")
