#ifndef __STORAGE_H
#define __STORAGE_H

#include "typedefs.h"

// Имя секции хранилища
#define STORAGE_SECTION     ".storage"

// Инициализация модуля
void storage_init(void);
// Сигнал о том, что данные изменились
void storage_modified(void);

// Обработчик прерывания от FPEC
void storage_interrupt_flash(void);

#endif // __STORAGE_H
