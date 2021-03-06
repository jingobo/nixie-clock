#include <romfs.h>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <windows.h>

using namespace std;

// Реализация сброщика образа файловой системы
static class : romfs_t::builder_t
{
    // Путь к рабочей директории
    string basedir;
    // Выходной файл
    ofstream output;
    // Буфер для ввода/вывода
    char buffer[8 * 1024];

    // Вывод разделителя
    static void out_separator(void)
    {
        for (auto i = 0; i < 79; i++)
            cout << '-';
        cout << endl;
    }

    // Фукция обработчик файла
    bool execute_file(string path)
    {
        // Получаем относительный путь
        auto rel_path = path.c_str() + basedir.length();
        // Лог
        cout << "Input file: " << rel_path << endl;
        // Открытие файла
        ifstream input;
        input.open(path, ios_base::in | ios::binary | ios::ate);
        if (!input.is_open())
        {
            out_error("Unable to open input!");
            return false;
        }
        // Путь к файлу
        if (strlen(rel_path) >= romfs_t::PATH_SIZE_MAX)
        {
            out_error("Relative path too long!");
            return false;
        }
        // Размер
        if (input.tellg() > (romfs_t::size_t)-1)
        {
            out_error("Size too big!");
            return false;
        }
        auto size = (romfs_t::size_t)input.tellg();
        cout << "File size: " << size << endl;
        // Запись заголовка
        if (!file_new(rel_path, size))
            return false;
        // Запись файла
        input.seekg(0, ios::beg);
        while (!input.eof())
        {
            // Чтение блока из исходного файла
            input.read(buffer, sizeof(buffer));
            auto readed = input.gcount();
            if (readed <= 0)
                continue;
            // Запись блока в выходной файл
            if (!file_write(buffer, (romfs_t::size_t)readed))
                return false;
        }
        // Закрытие исходжного файла
        input.close();
        // Завершение файла
        return file_finalize();
    }

    // Рекурсивная функция обработки директории
    bool execute_directory(string path)
    {
        path += '/';
        // Получаем относительный путь
        auto rel_path = path.c_str() + basedir.length();
        // Лог
        cout << "Check directory: " << rel_path << endl;
        // Начало поиска файлов и директорий
        WIN32_FIND_DATA find_info;
        auto find_handle = FindFirstFile((path + "*.*").c_str(), &find_info);
        if (find_handle == INVALID_HANDLE_VALUE)
        {
            // Что то пошло не так, как минимум директории ".." и "." должны быть
            out_error("Failed to scan directory!");
            return false;
        }
        out_separator();
        do
        {
            string name(find_info.cFileName);
            // Скрытое?
            if (find_info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
            {
                cout << "Skip hidden object: " << name << endl;
                continue;
            }
            // Директория?
            if (find_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // Корень и родительская директория
                if (name != "." && name != ".." && !execute_directory(path + name))
                    return false;
                continue;
            }
            // Файл?
            if (find_info.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
            {
                if (!execute_file(path + name))
                    return false;
                out_separator();
                continue;
            }
        } while (FindNextFileA(find_handle, &find_info));
        // Закрытие дескриптора поиска
        FindClose(find_handle);
        return true;
    }
protected:
    // Низкоуровневая запись (добавление данных в конец)
    virtual bool write(const void *source, size_t size)
    {
        output.write((const char *)source, size);
        if (output.fail())
        {
            out_error("Unable to write output");
            return false;
        }
        return true;
    }
public:
    // Вывод сообщения об ошибке
    static void out_error(const char *msg)
    {
        // Проверка аргументов
        assert(msg != NULL);
        // Вывод
        cout << "ERROR: " << msg << endl;
    }

    // Обработка файлов в указанной директории
    bool build(const char *rom_name)
    {
        // Проверка аргументов
        assert(rom_name != NULL);
        assert(strlen(rom_name) > 0);
        // Получаем полный путь к обрабатываемой директории
        char *fp;
        if (GetFullPathName(rom_name, MAX_PATH, buffer, &fp) == NULL || fp == NULL)
        {
            out_error("Invalid input path");
            return false;
        }
        // Заменяем слеши
        basedir = string(buffer);
        std::replace(basedir.begin(), basedir.end(), '\\', '/');
        cout << "Base directory: " << basedir << endl;
        // Открываем выходной файл
        output.open(basedir + ".rom", ios_base::out | ios::binary);
        if (!output.is_open())
        {
            out_error("Unable to open output");
            return false;
        }
        // Обход всех файлов в рабочей директории
        if (execute_directory(basedir) && total_finalize())
        {
            output.flush();
            cout << "Success! Total size: " << output.tellp() << " bytes." << endl;
            return true;
        }
        return false;
    }
} builder;

// Точка входа в приложение
int main(int argc, char *argv[])
{
    // Приветствие
    cout << "ROMFS packer. JingoBo (2018)" << endl;
    // Проверка аргументов
    if (argc != 2)
    {
        builder.out_error("Specify input path");
        return -2;
    }
    return builder.build(argv[1]) ? 0 : -1;
}
