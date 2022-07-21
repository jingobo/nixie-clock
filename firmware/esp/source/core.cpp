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

#include <esp_timer.h>

// Имя модуля для логирования
LOG_TAG_DECL("CORE");

// Процессор исходящих пакетов с внешних сторон
core_processor_out_t core_processor_out;

// Процессор входящих пактов со стороны ESP
static ipc_handler_host_t esp_processor_in;

// Список дескрипторов сторон связи
static const struct
{
    // Процессор ввода
    ipc_processor_t * in;
    // Имя (для логов)
    const char * name;
} CORE_LINK_SIDE[CORE_LINK_SIDE_COUNT] =
{
    // CORE_LINK_SIDE_ESP
    { &esp_processor_in, "ESP" },
    // CORE_LINK_SIDE_STM
    { &stm_processor_in, "STM" },
    // CORE_LINK_SIDE_WEB
    { &httpd_processor_in, "WEB" },
};

// Карта маршрутизации ответных пакетов
static bool core_route_map[IPC_OPCODE_LIMIT][CORE_LINK_SIDE_COUNT];

// Основная задача ядра
static class core_main_task_t : public os_task_base_t
{
protected:
    // Обработчик задачи
    virtual void execute(void) override final
    {
        for (;;)
        {
            // Ожидание простоя связи с ядром
            stm_wait_idle();

            // Опрос ESP обработчиков IPC
            mutex.enter();
                esp_processor_in.pool();
            mutex.leave();
        }
    }
public:
    // Конструктор по умолчанию
    core_main_task_t(void) : os_task_base_t("core", true)
    { }
} core_main_task;

RAM bool core_processor_out_t::side_t::transmit_to(const ipc_packet_t &packet, const args_t &args, core_link_side_t dest)
{
    // Определяем код команды
    const auto opcode = packet.dll.opcode;
    // Имя текущей стороны
    const auto this_name = CORE_LINK_SIDE[side].name;
    // Имя другой стороны
    const auto other_name = CORE_LINK_SIDE[dest].name;
    // Признак запроса
    const auto request = packet.dll.dir != IPC_DIR_RESPONSE;

    // Передача запроса
    if (CORE_LINK_SIDE[dest].in->packet_process(packet, args))
    {
        // Если не пакет последний
        if (packet.dll.more)
            return false;

        // Пакет последний, лог
        LOGI("%s - %s %d to %s (%d bytes)", this_name, request ? "request" : "response", opcode, other_name, args.size);
        // Помечаем с какой стороны пришел запрос
        core_route_map[opcode][request ? side : dest] = request;
        return false;
    }

    // Не удалось передать пакет, лог
    LOGW("%s - %s %d to %s failed!", this_name,  request ? "request" : "response", opcode, other_name);
    return true;
}

RAM bool core_processor_out_t::side_t::packet_process(const ipc_packet_t &packet, const args_t &args)
{
    // Результат выполнения
    auto result = true;
    // Определяем код команды
    const auto opcode = packet.dll.opcode;

    // Начало ввода
    if (args.first)
        core_main_task.mutex.enter();

    // Первым делом проверка на направление
    switch (packet.dll.dir)
    {
        case IPC_DIR_REQUEST:
            {
                // Определяем кому передать
                core_link_side_t dest;
                if (opcode > IPC_OPCODE_ESP_HANDLE_BASE)
                    dest = CORE_LINK_SIDE_ESP;
                else if (opcode > IPC_OPCODE_STM_HANDLE_BASE)
                    dest = CORE_LINK_SIDE_STM;
                else
                {
                    // WTF?
                    LOGE("Unknown opcode group %d!", opcode);
                    return false;
                }

                // Передача
                if (transmit_to(packet, args, dest))
                    result = false;
            }
            break;

        case IPC_DIR_RESPONSE:
            // Определяем кому разослать
            for (auto dest = CORE_LINK_SIDE_ESP; dest < CORE_LINK_SIDE_COUNT; dest = ENUM_VALUE_NEXT(dest))
            {
                // Пропускаем того кто отвечает
                if (dest == side)
                    continue;

                // Нужно ли обрабатывать запрос
                if (!core_route_map[opcode][dest])
                    continue;

                // Передача
                if (transmit_to(packet, args, dest))
                    result = false;
            }
            break;

        default:
            assert(false);
            return false;
    }

    // Завершение ввода
    if (!(packet.dll.more && result))
    {
        core_main_task.mutex.leave();
        // Индикация
        // io_led_yellow.flash();
    }

    return result;
}

// Точка входа
extern "C" void app_main(void)
{
    // Переход на 160 МГц
    rtc_clk_cpu_freq_set(RTC_CPU_FREQ_160M);
    // Старт софтовых таймеров
    ESP_ERROR_CHECK(esp_timer_init());

    // Приветствие
    LOGI("=== NixieClock communication backend ===");
    LOGI("IDF version: %s", esp_get_idf_version());

    // Вывод информации о памяти
    LOGH();
    // Отчистка карты маршрутизации
    MEMORY_CLEAR_ARR(core_route_map);

    // Инициализация модулей
    io_init();
    fs_init();
    wifi_init();
    ntime_init();
    httpd_init();

    // Инициализация связи с STM
    stm_init();
    // Старт основной задачи
    core_main_task.start();
}

// Получает текущее значение тиков (реализуется платформой)
ipc_handler_t::tick_t ipc_handler_t::tick_get(void)
{
    return (tick_t)(esp_timer_get_time() / 1000);
}

// Получает процессор для передачи (реализуется платформой)
ipc_processor_t & ipc_handler_t::transmitter_get(void)
{
    return core_processor_out.esp;
}

void core_handler_add(ipc_handler_t &handler)
{
    esp_processor_in.handler_add(handler);
}
