#include "io.h"
#include "os.h"
#include "fs.h"
#include "log.h"
#include "stm.h"
#include "core.h"
#include "ntime.h"

#include "svc/wifi.h"
#include "svc/ntime.h"
#include "svc/httpd.h"

// Имя модуля для логирования
LOG_TAG_DECL("CORE");

// Карта маршрутизации ответных пакетов
static struct core_route_t
{
    // Для синхронизации
    os_mutex_t sync;
    // Таблица сторон с которых прилетел запрос
    bool map[IPC_OPCODE_LIMIT][CORE_LINK_SIDE_COUNT];

    // Конструктор по умолчанию
    core_route_t(void)
    {
        // Отчистка карты маршрутизации
        memset(map, 0, sizeof(map));
    }
} core_route;

// Основная задача ядра
core_main_task_t core_main_task;
// Процессор исходящих пакетов с внешних сторон
core_processor_out_t core_processor_out;

// Процессор входящих пактов со стороны ESP
static ipc_packet_glue_t esp_processor_in;
// Процессоры входящих пакетов
static ipc_processor_t * core_processor_in[CORE_LINK_SIDE_COUNT] =
{
    // CORE_LINK_SIDE_ESP
    &esp_processor_in,
    // CORE_LINK_SIDE_STM
    &stm_processor_in,
    // TODO: CORE_LINK_SIDE_WEB
    NULL,
};

// Имена сторон (для логов)
static const char * const CORE_SIDE_NAME[] =
{
    "ESP",
    "STM",
    "WEB",
};

RAM void core_processor_out_t::side_t::packet_process_sync_accuire(bool get)
{
    if (get)
        core_route.sync.enter();
    else
        core_route.sync.leave();
}

RAM ipc_processor_status_t core_processor_out_t::side_t::packet_split(ipc_opcode_t opcode, ipc_dir_t dir, const void *source, size_t size)
{
    core_route.sync.enter();
        auto result = ipc_processor_t::packet_split(opcode, dir, source, size);
        core_route.sync.leave();
    return result;
}

RAM ipc_processor_status_t core_processor_out_t::side_t::packet_process(const ipc_packet_t &packet, const ipc_processor_args_t &args)
{
    auto result = IPC_PROCESSOR_STATUS_OVERLOOK;
    // Определяем код команды
    auto opcode = packet.dll.opcode;
    // Определяем последний ли пакет
    auto last = !packet.dll.more;
    // Первым делом проверка на направление
    switch (packet.dll.dir)
    {
        case IPC_DIR_REQUEST:
            {
                // Конечная сторона
                core_link_side_t dest = CORE_LINK_SIDE_COUNT;
                // Определяем кому передать
                if (opcode > IPC_OPCODE_ESP_HANDLE_BASE)
                    dest = CORE_LINK_SIDE_ESP;
                else if (opcode > IPC_OPCODE_STM_HANDLE_BASE)
                    dest = CORE_LINK_SIDE_STM;
                else
                {
                    // WTF?
                    LOGW("Unknown opcode group %d!", opcode);
                    result = IPC_PROCESSOR_STATUS_CORRUPTION;
                    break;
                }
                assert(dest != CORE_LINK_SIDE_COUNT);
                // Если пакет последний
                if (last)
                {
                    // Индикация
                    io_led_yellow.flash();
                    // Лог
                    LOGI("%s request 0x%02x, %d bytes", CORE_SIDE_NAME[dest], packet.dll.opcode, args.size);
                }
                // Передача
                result = core_processor_in[dest]->packet_process(packet, args);
                // Если результат ОК и это последний пакет...
                if (result == IPC_PROCESSOR_STATUS_SUCCESS && last)
                    // Помечаем с какой стороны пришел запрос
                    core_route.map[opcode][side] = true;
            }
            break;
        case IPC_DIR_RESPONSE:
            {
                // Определяем кому разослать
                for (auto lside = CORE_LINK_SIDE_ESP; lside < CORE_LINK_SIDE_COUNT; lside = ENUM_VALUE_NEXT(lside))
                {
                    // Пропускаем того кто отвечает
                    if (lside == side)
                        continue;
                    // Нужно ли обрабатывать запрос
                    if (!core_route.map[opcode][lside])
                        continue;
                    // Лог
                    LOGI("%s response 0x%02x, %d bytes", CORE_SIDE_NAME[lside], packet.dll.opcode, args.size);
                    // Передача
                    auto status = core_processor_in[lside]->packet_process(packet, args);
                    // Если передать не удалось или это последний пакет...
                    if (status != IPC_PROCESSOR_STATUS_SUCCESS || last)
                        // Снимаем метку
                        core_route.map[opcode][lside] = false;
                    // Готовим ответ
                    if (result > status)
                        result = status;
                }
                // Индикация
                if (last)
                    io_led_yellow.flash();
            }
            break;
        default:
            assert(false);
            break;
    }
    // Лог
    if (result != IPC_PROCESSOR_STATUS_SUCCESS)
        LOGW("Packet process failed! Code: %d.", result);
    return result;
}

void core_add_command_handler(ipc_command_handler_t &handler)
{
    core_route.sync.enter();
        esp_processor_in.add_command_handler(handler);
    core_route.sync.leave();
}

core_main_task_t::core_main_task_t(void) :
    os_task_base_t("core"),
    deffered_call_queue(xQueueCreate(15, sizeof(callback_t)))
{ }

void core_main_task_t::deffered_call(callback_t callback)
{
     auto result = xQueueSend(deffered_call_queue, &callback, OS_MS_TO_TICKS(1000));
     assert(result == pdTRUE);
     UNUSED(result);
}

void core_main_task_t::execute(void)
{
    for (;;)
    {
        // Обработка отложенных вызовов
        callback_t callback;
        if (xQueueReceive(deffered_call_queue, &callback, portMAX_DELAY))
            callback();
    }
}

// Точка входа
extern "C" void app_main(void)
{
    // Переход на 160 МГц
    rtc_clk_cpu_freq_set(RTC_CPU_FREQ_160M);
    // Приветствие
    LOGI("=== NixieClock communication backend ===");
    LOGI("IDF version: %s", esp_get_idf_version());
    // Вывод информации о памяти
    LOGH();
    // Старт основной задачи
    core_main_task.start();
    // Инициализация модулей
    io_init();
    fs_init();
    wifi_init();
    ntime_init();
    //httpd_init();
    // Инициализация связи с STM (последним)
    stm_init();
}
