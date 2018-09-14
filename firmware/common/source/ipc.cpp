#include "ipc.h"

// Заполнитель для отсутствущих данных
#define IPC_FILLER      0xFF
// Значение магическое числа начала пакета
#define IPC_MAGIC       0xAA

ROM bool ipc_command_data_t::decode_buffer(ipc_dir_t dir, const void *buffer, size_t size)
{
    // Получаем буфер и его размер
    auto psize = buffer_size(dir);
    auto ptr = (void *)buffer_pointer(dir);
    // Проверка длинны
    if (psize < size)
        return false;
    // Копирование данных
    memcpy(ptr, buffer, size);
    // Декодирование
    return decode(dir, size);
}

RAM bool ipc_command_data_t::decode_packet(const ipc_packet_t &packet)
{
    return !packet.dll.more && decode_buffer(packet.dll.dir, packet.apl.u8, packet.dll.length);
}

RAM size_t ipc_command_data_empty_t::buffer_size(ipc_dir_t dir) const
{
    UNUSED(dir);
    return 0;
}

RAM const void * ipc_command_data_empty_t::buffer_pointer(ipc_dir_t dir) const
{
    UNUSED(dir);
    return NULL;
}

RAM size_t ipc_command_data_empty_t::encode(ipc_dir_t dir) const
{
    UNUSED(dir);
    assert(dir == IPC_DIR_REQUEST);
    return 0;
}

RAM bool ipc_command_data_empty_t::decode(ipc_dir_t dir, size_t size)
{
    return dir == IPC_DIR_REQUEST && size == 0;
}

RAM void ipc_handler_event_t::idle(void)
{ }

RAM void ipc_handler_event_t::reset(void)
{ }

ROM ipc_slots_t::ipc_slots_t(void)
{
    for (auto i = 0; i < ARRAY_SIZE(slots); i++)
        slots[i].link(unused);
}

ROM void ipc_slots_t::use(ipc_slot_t &slot)
{
    // Проверка состояния
    assert(slot.list() == &unused);
    // Смена списков
    slot.unlink();
    slot.link(used);
}

ROM void ipc_slots_t::free(ipc_slot_t &slot)
{
    // Проверка состояния
    assert(slot.list() == &used);
    // Смена списков
    slot.unlink();
    slot.link(unused);
}

ROM void ipc_slots_t::clear(void)
{
    while (!used.empty())
        free(*used.last());
}

ROM ipc_controller_t::ipc_controller_t(void)
{
    // Установка магического поля
    for (auto i = tx.unused.head(); i != NULL; i = list_item_t::next(i))
        i->dll.magic = IPC_MAGIC;
}

ROM void ipc_controller_t::clear_slots(void)
{
    tx.clear();
    rx.clear();
}

ROM ipc_handler_command_t * ipc_controller_t::handler_command_find(ipc_command_t command) const
{
    // Поиск обработчика с такой командой
    for (auto i = command_handlers.head(); i != NULL; i = list_item_t::next(i))
        if (i->data_get().command == command)
            return i;
    return NULL;
}

ROM uint8_t ipc_controller_t::checksum(const void *source, uint8_t size)
{
    auto result = size;
    auto src = (const uint8_t *)source;
    for (; size > 0; size--, src++)
        result += *src;
    return result;
}

ROM void ipc_controller_t::transmit_flow(ipc_command_flow_request_t::reason_t reason)
{
    command_flow.request.reason = reason;
    auto result = transmit(IPC_DIR_REQUEST, command_flow);
    assert(result);
    UNUSED(result);
}

ROM void ipc_controller_t::reset_layer(ipc_command_flow_request_t::reason_t reason, bool internal)
{
    assert(reason > ipc_command_flow_request_t::REASON_NOP);
    // Оповещение
    for (auto i = event_handlers.head(); i != NULL; i = list_item_t::next(i))
        i->reset();
    // Сброс всех слотов
    clear_slots();
    // Отправка команды
    if (internal)
        transmit_flow(reason);
}

ROM void ipc_controller_t::handler_add_event(ipc_handler_event_t &handler)
{
    // Поиск обработчика с таким адресом
    assert(!event_handlers.contains(handler));
    // Добавление
    handler.link(event_handlers);
}

ROM void ipc_controller_t::handler_add_command(ipc_handler_command_t &handler)
{
    // Поиск обработчика с такой командой
    assert(handler_command_find(handler.data_get().command) == NULL);
    // Добавление
    handler.link(command_handlers);
}

ROM bool ipc_controller_t::receive_prepare(const ipc_packet_t &packet, receive_args_t &args)
{
    // Поиск обработчика по команде
    auto handler = handler_command_find(packet.dll.command);
    assert(handler != NULL);
    // Получаем команду
    auto &data = handler->data_get();
    // Проверка длинны
    auto dir = packet.dll.dir;
    if (data.buffer_size(dir) < args.size)
        return false;
    // Заполняем аргументы
    args.cookie = handler;
    args.buffer = (void *)data.buffer_pointer(dir);
    return true;
}

ROM bool ipc_controller_t::receive_finalize(const ipc_packet_t &packet, const receive_args_t &args)
{
    // Проверка состояния
    assert(args.cookie != NULL);
    // Получаем обработчик
    auto dir = packet.dll.dir;
    auto &handler = *(ipc_handler_command_t *)args.cookie;
    // Декодирование
    if (!handler.data_get().decode(dir, args.size))
        return false;
    // Оповешение
    handler.notify(dir);
    return true;
}

ROM bool ipc_controller_t::transmit_raw(ipc_dir_t dir, ipc_command_t command, const void *source, size_t size)
{
    // Проверка аргументов
    assert(size <= 0 || source != NULL);
    // Проверяем на размер
    auto slot_count = (size <= 0) ? 1 : DIV_CEIL(size, IPC_APL_SIZE);
    if (tx.unused.count() < slot_count)
        return false;
    // Проверяем, есть ли такая команда с таким же направлением
    for (auto i = tx.used.head(); i != NULL; i = list_item_t::next(i))
        if (i->dll.command == command && i->dll.dir == dir)
            return false;
    // Рзбиваем на слоты
    for (auto src = (const uint8_t *)source; slot_count > 0; slot_count--)
    {
        auto &item = *tx.unused.head();
        auto more = size > IPC_APL_SIZE;
        auto len = more ? IPC_APL_SIZE : (uint8_t)size;
        // Заполнение команды и опций
        item.dll.command = command;
        item.dll.dir = dir;
        item.dll.more = more;
        item.dll.length = len;
        // Полезные данные
        memcpy(&item.apl, src, len);
        // Перенос в список используемых
        tx.use(item);
        // Переход к следующим данным
        size -= len;
        src += len;
    }
    return true;
}

ROM bool ipc_controller_t::transmit(ipc_dir_t dir, const ipc_command_data_t &data)
{
    return transmit_raw(dir, data.command, data.buffer_pointer(dir), data.encode(dir));
}

ROM void ipc_controller_t::packet_output(ipc_packet_t &packet)
{
    // Проверяем, есть ли данные
    for (auto try_idle = false;;)
    {
        // Получаем первый слот
        auto head = tx.used.head();
        if (head != NULL)
        {
            // Перенос слота в не используемые
            tx.free(*head);
            packet = *head;
            break;
        }
        if (try_idle)
        {
            // Нет команды
            transmit_flow(ipc_command_flow_request_t::REASON_NOP);
            continue;
        }
        // Обход обработчиков простоя
        for (auto i = event_handlers.head(); i != NULL;)
        {
            i->idle();
            // Если ничего не записал...
            if (tx_empty())
            {
                i = list_item_t::next(i);
                continue;
            }
            // Что то было записано, этот обрабочтик в конец
            i->unlink();
            i->link(event_handlers);
            break;
        }
        try_idle = true;
    }
    auto len = packet.dll.length;
    // Заполнение DLL полей
    packet.dll.fast = tx.used.count() > 0;
    packet.dll.checksum = checksum(&packet.apl, len);
    // Заполнение пустых данных
    memset(packet.apl.u8 + len, IPC_FILLER, IPC_APL_SIZE - len);
}

ROM void ipc_controller_t::packet_input(const ipc_packet_t &packet)
{
    auto temp = rx.unused.head();
    // Если нет свободных
    if (temp == NULL)
    {
        reset_layer(ipc_command_flow_request_t::REASON_OVERFLOW);
        return;
    }
    // Валидация
    {
        auto len = packet.dll.length;
        if (packet.dll.magic != IPC_MAGIC || len > IPC_APL_SIZE ||  // Проверка магического поля и длинны
            packet.dll.checksum != checksum(&packet.apl, len))      // Проверка контрольной суммы
        {
            reset_layer(ipc_command_flow_request_t::REASON_CORRUPTION);
            return;
        }
    }
    // Обработка команды управления потоком
    if (packet.dll.command == command_flow.command)
    {
        // Декодирование
        if (!command_flow.decode_packet(packet))
        {
            reset_layer(ipc_command_flow_request_t::REASON_BAD_CONTENT);
            return;
        }
        // Обработка
        auto reason = command_flow.request.reason;
        if (reason > ipc_command_flow_request_t::REASON_NOP)
            reset_layer(reason, false);
        return;
    }
    // Используем
    rx.use(*temp);
    // Копирование пакета
    temp->assign(packet);
    // Последний пакет?
    if (packet.dll.more)
        return;
    // Определение команды и направления
    auto dir = packet.dll.dir;
    auto command = packet.dll.command;
    // Подсчет общего количества байт
    size_t size = 0;
    for (auto i = rx.used.head(); i != NULL; i = list_item_t::next(i))
        // Фильтрация по команде и направлению
        if (i->dll.command == command && i->dll.dir == dir)
            size += i->dll.length;
    // Оповешение о начале сброки
    receive_args_t args(size);
    if (!receive_prepare(packet, args))
    {
        reset_layer(ipc_command_flow_request_t::REASON_BAD_CONTENT);
        return;
    }
    // Может быть что буфер не указан, но только если и размер 0
    assert(args.buffer != NULL || args.size == 0);
    // Сборка пакета
    auto buffer = (uint8_t *)args.buffer;
    for (auto i = rx.used.head(); i != NULL;)
    {
        // Фильтрация по команде и направлению
        if (i->dll.command != command || i->dll.dir != dir)
        {
            i = list_item_t::next(i);
            continue;
        }
        auto len = i->dll.length;
        // Копирование данных, смещения
        memcpy(buffer, &i->apl, len);
        buffer += len;
        // Переход к следующему слоту
        auto tmp = i;
        i = list_item_t::next(i);
        // Освобождение слота
        rx.free(*tmp);
    }
    // Вызов события о завершении сборки пакета
    if (!receive_finalize(packet, args))
    {
        reset_layer(ipc_command_flow_request_t::REASON_BAD_CONTENT);
        return;
    }
}

// Заполнение всех байт пакета байтом заполнения
RAM void ipc_controller_t::packet_clear(ipc_packet_t &packet)
{
    memset(&packet, IPC_FILLER, sizeof(packet));
}
