#ifndef __TYPEDEFS_H
#define __TYPEDEFS_H

// Common
#include <common.h>
// IAR
#include <intrinsics.h>

// ������� � ������ ����� �� ����������
#define WFI()                   __WFI()
// ��������� ����� ����������� ����� ������
#define PRAGMA_OPTION(m, v)      PRAGMA(m = v)

// ������ ����������� �������
#define OPTIMIZE_NONE           PRAGMA_OPTION(optimize, none)
#define OPTIMIZE_SIZE           PRAGMA_OPTION(optimize, size)
#define OPTIMIZE_SPEED          PRAGMA_OPTION(optimize, speed)
// ������������ �������
#define INLINE_NEVER            PRAGMA_OPTION(inline, never)
#define INLINE_FORCED           PRAGMA_OPTION(inline, forced)

// ���������/���������� ��������������
#define WARNING_SUPPRESS(err)   PRAGMA_OPTION(diag_suppress, err)
#define WARNING_DEFAULT(err)    PRAGMA_OPTION(diag_default, err)

// ������������ ������
#define ALIGN_DATA_8            PRAGMA_OPTION(data_alignment, 1)
#define ALIGN_DATA_16           PRAGMA_OPTION(data_alignment, 2)
#define ALIGN_DATA_32           PRAGMA_OPTION(data_alignment, 4)

// ����������� ��� ����������
#define IRQ_ROUTINE             INLINE_NEVER OPTIMIZE_SPEED
// ��������� ������� ��������� ����������
#define IRQ_CTX_SAVE()          __istate_t __istate = __get_interrupt_state()
// ���������� ����������� ��������� ����������
#define IRQ_CTX_RESTORE()       __set_interrupt_state(__istate)
// ��������� ��� ������������ ����������
#define IRQ_CTX_DISABLE()       __disable_interrupt()

// ����/����� � ����������  �� ���������� ���
#define IRQ_SAFE_ENTER()        \
    IRQ_CTX_SAVE();             \
    IRQ_CTX_DISABLE()
#define IRQ_SAFE_LEAVE()        IRQ_CTX_RESTORE()

#endif // __TYPEDEFS_H
