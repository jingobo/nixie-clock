#ifndef __ROMFS_H
#define __ROMFS_H

#include "common.h"

// ����� ���������� �������� �������
struct romfs_t
{
    // ��� ���������� ��� �������� � ��������
    typedef uint32_t size_t;
    // ���������
    enum 
    { 
        // ������ ������������ ������
        ALIGN_DATA_SIZE = sizeof(size_t),
        // ������������ ������ ���� ����� ������� ������������ ������
        PATH_SIZE_MAX = ALIGN_DATA_SIZE * 16,
    };
private:
    // �������� �����������
    romfs_t(void)
    { }

    // ������ ��� �����
    typedef uint8_t pads_t[ALIGN_DATA_SIZE];

#ifndef NDEBUG
    // �������� ����
    static bool check_path(const char *path);
#endif

    // ��������� �����
    struct header_t
    {
        // ���� � �����
        char path[PATH_SIZE_MAX];
        // ������
        size_t size;

        // ����������� �� ���������
        header_t(void) : size(0)
        {
            CSTR_CLEAR(path);
        }

        // ����������� � ��������� ������
        header_t(const char *_path, size_t _size) : size(_size)
        {
            assert(check_path(_path));
            strcpy(path, _path);
        }

        // �������� ���������� ����� � ����� �����
        uint8_t pads(void) const
        {
            return size % ALIGN_DATA_SIZE;
        }

        // �������� ������ ����� � ������
        size_t size_pads(void) const
        {
            return size + pads();
        }
    };
public:
    // ����� ��� ������ �������� �������
    class reader_t
    {
        // ��������� ���������
        header_t header;
    protected:
        // �������������� ������ �� ���������� ��������
        virtual bool read(void *dest, size_t size, size_t offset) = 0;
    public:
        // ���������� �����
        class handle_t
        {
            friend class reader_t;
            // ��������� �� ������������ �����
            reader_t *reader;
            // ������� �������� � ������ �����
            size_t base, fsize;
            // �������� ��� ������
            size_t offset;

            // �������� �����������
            handle_t(reader_t &reader, size_t base, size_t size)
            {
                offset = 0;
                this->base = base;
                this->fsize = size;
                this->reader = &reader;
            }
        public:
            // ����������� �� ����������
            handle_t(void) : reader(NULL)
            { }

            // ��������, ������ �� ����
            bool opened(void) const
            {
                return reader != NULL;
            }

            // �������� ������ �����
            size_t size(void) const
            {
                // ��������� ��� assert, ���� � �������������
                return opened() ? fsize : 0;
            }

            // �������� �����
            bool close(void)
            {
                if (opened())
                {
                    reader = NULL;
                    return true;
                }
                return false;
            }

            // ������ �����
            bool read(void *dest, size_t size);

            // ������� � ������ ��������
            bool seek(size_t offset);
        };

        // �������� �����
        handle_t open(const char *path);
    };

    // ����� ��� ������������ �������� �������
    class builder_t
    {
        // ������ ���������� ����� ���������� ������������� �����
        uint8_t pads;
    protected:
        // �������������� ������ (���������� ������ � �����)
        virtual bool write(const void *source, size_t size) = 0;
    public:
        // ������ ������ �����
        bool file_new(const char *path, size_t size);

        // ������ ����������� �����
        bool file_write(const void *source, size_t size);

        // ���������� �����
        bool file_finalize(void);
 
        // ������ ����������
        bool total_finalize(void);
    };
};

#endif // __ROMFS_H
