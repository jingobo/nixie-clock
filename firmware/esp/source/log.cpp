#include "log.h"
#include "tool.h"

void log_heap(const char *module)
{
    tool_bts_buffer_t buf;
    tool_byte_to_string(esp_get_free_heap_size(), buf);
    ESP_LOGI(module, "Free heap size %s", buf);
}
