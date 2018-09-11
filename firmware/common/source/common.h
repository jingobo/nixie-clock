#ifndef __COMMON_H
#define __COMMON_H

// ����������� ����������
#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// ��������� �� ����������
#include <platform.h>

// ������������ ���� ������
typedef float float32_t;
typedef double float64_t;
// ������������ ������������ ��� �� ���������
typedef float32_t float_t;

// ������������ ���� � ����
#define SAFE_BLOCK(code)        \
    do {                        \
        code;                   \
    } while (false)

// --- ��� ���������������� � ����� ����������� --- //

// ������ ������
#define MACRO_EMPTY
// ������� ������
#define E_SYMBOL                extern
// ������ ���������� � C-style
#define C_SYMBOL                extern "C"
// �������������� � ������ �������� �������
#define STRINGIFY(x)            #x
// ����� ����������� (������) ��� ������
#define PRAGMA(msg)             _Pragma(STRINGIFY(msg))
// ������ ����
#define BLOCK_EMPTY             SAFE_BLOCK(MACRO_EMPTY)

// --- �������������� �������� --- //
        
// ��������� �� 1000
#define XK(v)                	(1000l * (v))
// ��������� �� 1000000
#define XM(v)                	(1000000l * (v))
// �������� ������ ������� � ���������
#define ARRAY_SIZE(x)           (sizeof(x) / sizeof((x)[0]))
// ������� � ����������� � ������� �������
#define DIV_CEIL(x, y)          ((x) / (y) + ((x) % (y) > 0 ? 1 : 0))
// �������� ��� ������������ (����������� �������)
#define ENUM_DX(v, dx)          ((decltype(v))(((int)(v)) + (dx)))
#define ENUM_INC(v)             ENUM_DX(v, 1)
#define ENUM_DEC(v)             ENUM_DX(v, -1)

// --- ������� ����� --- //
        
// �������� ������� �����
#define MASK(type, value, pos)  ((type)(value) << (pos))
#define MASK_8(value, pos)     	MASK(uint8_t, value, pos)
#define MASK_16(value, pos)     MASK(uint16_t, value, pos)
#define MASK_32(value, pos)     MASK(uint32_t, value, pos)

// �� ������������ ������
#define UNUSED(x)           	((void)(x))
// �������� FALSE
#define WAIT_WHILE(expr)         do { } while (expr)
// �������� TRUE
#define WAIT_FOR(expr)           WAIT_WHILE(!(expr))

// ������������ ����� � ����������
#define ALIGN_FIELD_8           PRAGMA(pack(1))
#define ALIGN_FIELD_16          PRAGMA(pack(2))
#define ALIGN_FIELD_32          PRAGMA(pack(4))
#define ALIGN_FIELD_DEF         PRAGMA(pack())

// �������� ������
#define MEMORY_CLEAR(var)       memset(&(var), 0, sizeof(var))
// �������� ������ �� ���������
#define MEMORY_CLEAR_PTR(var)   memset(var, 0, sizeof(*(var)))

// ��������� �������
class notify_t
{
public:
    // ������� ����������
    virtual void notify_event(void) = 0;
};

#endif // __TYPEDEFS_H
