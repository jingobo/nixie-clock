#ifndef __FS_H
#define __FS_H

#include <romfs.h>

// Дескрипатор файла
typedef romfs_t::reader_t::handle_t fs_file_t;

// Инициализация модуля
void fs_init(void);
// Открытие файла
fs_file_t fs_open(const char *path);

#endif // __FS_H
