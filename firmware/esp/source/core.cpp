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

RAM bool core_processor_out_t::side_t::packet_process(const ipc_packet_t &packet, const args_t &args)
{
    auto result = false;
    // Определяем код команды
    auto opcode = packet.dll.opcode;
    // Определяем последний ли пакет
    auto last = !packet.dll.more;

    // Начало ввода
    if (args.first)
        core_main_task.mutex.enter();

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
                    break;
                }
                assert(dest != CORE_LINK_SIDE_COUNT);
                // Если пакет последний
                if (last)
                {
                    // Индикация
                    io_led_yellow.flash();
                    // Лог
                    LOGI("%s request 0x%02x, %d bytes", CORE_LINK_SIDE[dest].name, packet.dll.opcode, args.size);
                    // Помечаем с какой стороны пришел запрос
                    core_route_map[opcode][side] = true;
                }
                // Передача
                result = CORE_LINK_SIDE[dest].in->packet_process(packet, args);
                // Если результат бедовый и это последний пакет...
                if (last && !result)
                    // Сброс
                    core_route_map[opcode][side] = false;
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
                    if (!core_route_map[opcode][lside])
                        continue;
                    // Лог
                    LOGI("%s response 0x%02x, %d bytes", CORE_LINK_SIDE[lside].name, packet.dll.opcode, args.size);
                    // Передача
                    result = CORE_LINK_SIDE[lside].in->packet_process(packet, args);
                    // Если передать не удалось или это последний пакет...
                    if (last || !result)
                        // Снимаем метку
                        core_route_map[opcode][lside] = false;
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
    // Завершение ввода
    if (last || !result)
        core_main_task.mutex.leave();

    // Лог
    if (!result)
        LOGW("Packet process failed! Opcode: %d, direction: %d", opcode, packet.dll.dir);
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
    memset(core_route_map, 0, sizeof(core_route_map));

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
