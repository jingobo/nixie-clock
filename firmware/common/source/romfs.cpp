#include "romfs.h"

#ifndef NDEBUG
// �������� ����
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
    // ���� ���� �� ������
    if (!opened())
        // ��������� ��� assert, ���� � �������������
        return false;
    // ���� ����������� ������ ��� ����
    if (size > fsize - offset)
        // ��������� ��� assert, ���� � �������������
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
    // ���� ���� �� ������
    if (!opened())
        // ��������� ��� assert, ���� � �������������
        return false;
    // ���� �������� ������ ��� ����
    if (offset > fsize)
        // ��������� ��� assert, ���� � �������������
        return false;
    this->offset = offset;
    return true;
}

// �������� �����
romfs_t::reader_t::handle_t romfs_t::reader_t::open(const char *path)
{
    // �������� ����������
    assert(check_path(path));
    // ����� ���������
    for (size_t offset = 0;;)
    {
        // ������ ���������
        if (!read(&header, sizeof(header), offset))
            break;
        offset += sizeof(header_t);
        // ���� �������� �����
        if (strlen(header.path) <= 0)
            break;
        // ��������� ����
        if (!strcmp(header.path, path))
            // �����
            return handle_t(*this, offset, header.size);
        // ������� � ���������� �����
        offset += header.size_pads();
    }
    // ������
    return handle_t();
}

bool romfs_t::builder_t::file_new(const char *path, size_t size)
{
    // �������� ����������
    assert(check_path(path));
    // ������������� ���������
    header_t header(path, size);
    // ���������� ���������� �����
    pads = header.pads();
    // ������ ���������
    return write(&header, sizeof(header));
}

bool romfs_t::builder_t::file_write(const void *source, size_t size)
{
    // �������� ����������
    assert(source != NULL);
    // ������ ��������
    return write(source, size);
}

bool romfs_t::builder_t::file_finalize(void)
{
    // ��� ����
    pads_t dummy;
	memory_clear(dummy, sizeof(dummy));
	
    // ������
    return write(dummy, pads);
}

bool romfs_t::builder_t::total_finalize(void)
{
    // ������������� ������� ���������
    header_t header;
    // ������ ���������
    return write(&header, sizeof(header));
}
