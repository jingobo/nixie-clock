#include "fs.h"
#include "os.h"
#include "log.h"
#include "tool.h"
#include <esp_partition.h>

// Имя модуля для логирования
LOG_TAG_DECL("FS");

// Указатель на системный раздел в котором хранится ФС
static const esp_partition_t *fs_partition;

// Реализация читальщика файловой системы
static class fs_impl_t : public romfs_t::reader_t
{
    os_mutex_t fs_sync;
protected:
    // Низкоуровневое чтение по указанному смещению
    virtual bool read(void *dest, size_t size, size_t offset)
    {
        if (fs_partition == NULL)
            return false;
        fs_sync.enter();
            auto result = esp_partition_read(fs_partition, offset, dest, size) == ESP_OK;
        fs_sync.leave();
        return result;
    }
} fs_impl;

void fs_init(void)
{
    fs_partition = NULL;
    // Поиск раздела по типу
    auto prt_iterator = esp_partition_find((esp_partition_type_t)0x40, (esp_partition_subtype_t)0x00, NULL);
    if (prt_iterator == NULL)
    {
        LOGE("Unable to find partition!");
        return;
    }
    // Получение указателя на раздел
    fs_partition = esp_partition_get(prt_iterator);
    assert(fs_partition != NULL);
    // Вывод информации о разделе
    tool_bts_buffer_t bts;
    UNUSED(bts);
    tool_byte_to_string(fs_partition->size, bts);
    LOGI("Found partition %s, offset 0x%08x, size %s", fs_partition->label, fs_partition->address, bts);
}

fs_file_t fs_open(const char *path)
{
    // Проверка аргументов
    assert(path != NULL);
    // Пробуем открыть
    auto result = fs_impl.open(path);
    // Лог
    if (!result.opened())
        LOGW("Open file \"%s\" failed!", path);
    // Результат
    return result;
}
