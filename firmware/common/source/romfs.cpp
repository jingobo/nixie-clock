#include "romfs.h"

#ifndef NDEBUG
// Проверка пути
RAM bool romfs_t::check_path(const char *path)
{
    if (path == NULL)
        return false;
    auto size = strlen(path);
    return size > 0 && size < PATH_SIZE_MAX;
}
#endif

RAM bool romfs_t::reader_t::handle_t::read(void *dest, size_t size)
{
    // Если файл не открыт
    if (!opened())
        // Допускаем без assert, хоть и подозрительно
        return false;
    // Если запрашивают больше чем есть
    if (size > fsize - offset)
        // Допускаем без assert, хоть и подозрительно
        return false;
    if (reader->read(dest, size, base + offset))
    {
        offset += size;
        return true;
    }
    return false;
}

RAM bool romfs_t::reader_t::handle_t::seek(size_t offset)
{
    // Если файл не открыт
    if (!opened())
        // Допускаем без assert, хоть и подозрительно
        return false;
    // Если смещение больше чем файл
    if (offset > fsize)
        // Допускаем без assert, хоть и подозрительно
        return false;
    this->offset = offset;
    return true;
}

// Открытие файла
romfs_t::reader_t::handle_t romfs_t::reader_t::open(const char *path)
{
    // Проверка аргументов
    assert(check_path(path));
    // Поиск заголовка
    for (size_t offset = 0;;)
    {
        // Читаем заголовок
        if (!read(&header, sizeof(header), offset))
            break;
        offset += sizeof(header_t);
        // Если достигли конца
        if (strlen(header.path) <= 0)
            break;
        // Проверяем путь
        if (!strcmp(header.path, path))
            // Нашли
            return handle_t(*this, offset, header.size);
        // Переход к следующему файлу
        offset += header.size_pads();
    }
    // Фиаско
    return handle_t();
}

bool romfs_t::builder_t::file_new(const char *path, size_t size)
{
    // Проверка аргументов
    assert(check_path(path));
    // Инициалзиация заголовка
    header_t header(path, size);
    // Определяем количество падов
    pads = header.pads();
    // Запись заголовка
    return write(&header, sizeof(header));
}

bool romfs_t::builder_t::file_write(const void *source, size_t size)
{
    // Проверка аргументов
    assert(source != NULL);
    // Запись напрямую
    return write(source, size);
}

bool romfs_t::builder_t::file_finalize(void)
{
    // Под пады
    pads_t dummy;
	memory_clear(dummy, sizeof(dummy));
	
    // Запись
    return write(dummy, pads);
}

bool romfs_t::builder_t::total_finalize(void)
{
    // Инициалзиация пустого заголовка
    header_t header;
    // Запись заголовка
    return write(&header, sizeof(header));
}
