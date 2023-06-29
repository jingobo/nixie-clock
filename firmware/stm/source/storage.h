#ifndef __STORAGE_H
#define __STORAGE_H

#include "typedefs.h"

// Имя секции хранилища
#define STORAGE_SECTION     ".storage"

// Сигнал о том, что данные изменились
void storage_modified(void);

#endif // __STORAGE_H
