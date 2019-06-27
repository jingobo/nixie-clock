#ifndef __ROMFS_H
#define __ROMFS_H

#include "common.h"

// Класс простейшей файловой системы
struct romfs_t
{
    // Тип переменной для размеров и смещений
    typedef uint32_t size_t;
    // Константы
    enum 
    { 
        // Размер выравнивания данных
        ALIGN_DATA_SIZE = sizeof(size_t),
        // Максимальный размер пути файла включая терминальный символ
        PATH_SIZE_MAX = ALIGN_DATA_SIZE * 16,
    };
private:
    // Закрытый конструктор
    romfs_t(void)
    { }

    // Массив для падов
    typedef uint8_t pads_t[ALIGN_DATA_SIZE];

#ifndef NDEBUG
    // Проверка пути
    static bool check_path(const char *path);
#endif

    // Заголовок файла
    struct header_t
    {
        // Путь к файлу
        char path[PATH_SIZE_MAX];
        // Размер
        size_t size;

        // Конструктор по умолчанию
        header_t(void) : size(0)
        {
            CSTR_CLEAR(path);
        }

        // Конструктор с указанием данных
        header_t(const char *_path, size_t _size) : size(_size)
        {
            assert(check_path(_path));
            strcpy(path, _path);
        }

        // Получает количество падов в конце файла
        uint8_t pads(void) const
        {
            return size % ALIGN_DATA_SIZE;
        }

        // Получает размер файла с падами
        size_t size_pads(void) const
        {
            return size + pads();
        }
    };
public:
    // Класс для чтения файловой системы
    class reader_t
    {
        // Временный заголовок
        header_t header;
    protected:
        // Низкоуровневое чтение по указанному смещению
        virtual bool read(void *dest, size_t size, size_t offset) = 0;
    public:
        // Дескриптор файла
        class handle_t
        {
            friend class reader_t;
            // Указатель на родительский класс
            reader_t *reader;
            // Базовое смещение и размер файла
            size_t base, fsize;
            // Смещение при чтении
            size_t offset;

            // Закрытый конструктор
            handle_t(reader_t &reader, size_t base, size_t size)
            {
                offset = 0;
                this->base = base;
                this->fsize = size;
                this->reader = &reader;
            }
        public:
            // Конструктор по умоглчанию
            handle_t(void) : reader(NULL)
            { }

            // Получает, открыт ли файл
            bool opened(void) const
            {
                return reader != NULL;
            }

            // Получает размер файла
            size_t size(void) const
            {
                // Допускаем без assert, хоть и подозрительно
                return opened() ? fsize : 0;
            }

            // Закрытие файла
            bool close(void)
            {
                if (opened())
                {
                    reader = NULL;
                    return true;
                }
                return false;
            }

            // Чтение файла
            bool read(void *dest, size_t size);

            // Переход к новому смещению
            bool seek(size_t offset);
        };

        // Открытие файла
        handle_t open(const char *path);
    };

    // Класс для формирования файловой системы
    class builder_t
    {
        // Хранит количество падов последнего записываемого файла
        uint8_t pads;
    protected:
        // Низкоуровневая запись (добавление данных в конец)
        virtual bool write(const void *source, size_t size) = 0;
    public:
        // Начало нового файла
        bool file_new(const char *path, size_t size);

        // Запись содержимого файла
        bool file_write(const void *source, size_t size);

        // Завершение файла
        bool file_finalize(void);
 
        // Полное завершение
        bool total_finalize(void);
    };
};

#endif // __ROMFS_H
