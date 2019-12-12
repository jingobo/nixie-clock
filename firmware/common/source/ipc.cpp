#include "ipc.h"

RAM uint16_t ipc_packet_t::checksum_get(void) const
{
    uint16_t result = 0;
    const uint16_t *data = &this->dll.checksum + 1;
    for (auto i = 0; i < (IPC_PKT_SIZE / 2) - 1; i++, data++)
    {
        result += *data * 0xAC4F;
        result ^= result >> 8;
    }
    return result;
}

RAM void ipc_packet_t::prepare(ipc_opcode_t opcode, ipc_dir_t dir)
{
    dll.dir = dir;
    dll.opcode = opcode;
}

ipc_slots_t::ipc_slots_t(void)
{
    for (auto i = 0; i < ARRAY_SIZE(slots); i++)
        slots[i].link(unused);
}

RAM void ipc_slots_t::use(ipc_slot_t &slot)
{
    // Проверка состояния
    assert(slot.list() == &unused);
    // Смена списков
    slot.unlink();
    slot.link(used);
}

RAM void ipc_slots_t::free(ipc_slot_t &slot)
{
    // Проверка состояния
    assert(slot.list() == &used);
    // Смена списков
    slot.unlink();
    slot.link(unused);
}

void ipc_slots_t::clear(void)
{
    phase = IPC_BOOL_FALSE;
    while (!used.empty())
        free(*used.last());
}

bool ipc_processor_t::data_split(ipc_processor_t &processor, ipc_opcode_t opcode, ipc_dir_t dir, const void *source, size_t size)
{
    // Проверка аргументов
    assert(source != NULL || size <= 0);
    // Подготовка аргументов
    args_t args(size);
    // Подготовка пакета
    ipc_packet_t packet;
    packet.prepare(opcode, dir);
    // Рзбиваем на пакеты
    for (auto src = (const uint8_t *)source; size > 0;)
    {
        auto more = size > IPC_APL_SIZE;
        auto len = more ? IPC_APL_SIZE : (uint8_t)size;
        // Заполнение длинны и опций
        packet.dll.length = len;
        packet.dll.more = more ? IPC_BOOL_TRUE : IPC_BOOL_FALSE;
        // Полезные данные
        memcpy(&packet.apl, src, len);
        // Переход к следующим данным
        size -= len;
        src += len;
        // Обработка
        if (!processor.packet_process(packet, args))
            return false;
        args.first = false;
    }
    return true;
}

RAM void ipc_link_t::transmit_flow(reset_reason_t reason)
{
    auto result = data_split(*this, IPC_OPCODE_FLOW, IPC_DIR_REQUEST, &reason, sizeof(reason));
    assert(result);
    UNUSED(result);
}

void ipc_link_t::reset_layer(reset_reason_t reason, bool internal)
{
    assert(reason > RESET_REASON_NOP);
    // Сброс слотов, полей
    reset();
    // Отправка команды
    if (!internal)
    {
        // Для выравнивания с фазой приёма
        tx.phase_switch();
        return;
    }
    transmit_flow(reason);
    reseting = true;
}

RAM bool ipc_link_t::check_phase(const ipc_packet_t &packet)
{
    // Проверка последовательности
    if (packet.dll.phase == rx.phase_switch())
        return false;
    // Возвращаем фазу
    rx.phase_switch();
    return true;
}

void ipc_link_t::reset(void)
{
    reseting = false;
    tx.clear();
    rx.clear();
}

void ipc_link_t::packet_output(ipc_packet_t &packet)
{
    for (;;)
    {
        // Получаем первый слот
        auto head = tx.used.head();
        if (head != NULL)
        {
            // Перенос слота в не используемые
            tx.free(*head);
            packet = head->packet;
            break;
        }
        // Бездействие
        transmit_flow(RESET_REASON_NOP);
    }
    // Заполнение DLL полей
    packet.dll.phase = tx.phase_switch();
    packet.dll.checksum = packet.checksum_get();
}

void ipc_link_t::packet_input(const ipc_packet_t &packet)
{
    // Если был отправлен сброс, "старый" пакет не обрабатываем
    if (reseting)
    {
        reseting = false;
        // Для выравнивания с фазой передачи
        rx.phase_switch();
        return;
    }
    // Извлекаем конечный пакет из очереди
    auto pslot = rx.unused.head();
    if (pslot == NULL)
    {
        // Если нет свободных
        reset_layer(RESET_REASON_OVERFLOW);
        return;
    }
    auto &slot = *pslot;
    // Валидация
    {
        if (packet.dll.length > IPC_APL_SIZE || 
            packet.dll.checksum != packet.checksum_get())
        {
            reset_layer(RESET_REASON_CORRUPTION);
            return;
        }
    }
    // Обработка команды управления потоком
    if (packet.dll.opcode == IPC_OPCODE_FLOW)
    {
        // Декодирование
        auto reason = (reset_reason_t)packet.apl.u8[0];
        if (packet.dll.more != IPC_BOOL_FALSE ||
            packet.dll.dir != IPC_DIR_REQUEST ||
            packet.dll.length != sizeof(reason) ||
            reason >= RESET_REASON_COUNT)
        {
            reset_layer(RESET_REASON_CORRUPTION);
            return;
        }
        // Обработка команды управления потоком
        if (reason > RESET_REASON_NOP)
            reset_layer(reason, false);
    }
    // Проверка фазы
    if (check_phase(packet))
        return;
    // Команда управления потоком уже обработана
    if (packet.dll.opcode == IPC_OPCODE_FLOW)
        return;
    // Используем
    rx.use(slot);
    // Копирование пакета
    slot.packet = packet;
}

void ipc_link_t::process_incoming_packets(ipc_processor_t &receiver)
{
    // Цикл пока не закончатся конечные пакеты
    for (;;)
    {
        // Поиск конечного пакета
        const ipc_packet_t *found = NULL;
        for (auto slot = rx.used.head(); slot != NULL; slot = LIST_ITEM_NEXT(slot))
            if (!slot->packet.dll.more)
            {
                found = &slot->packet;
                break;
            }
        if (found == NULL)
            break;
        // Определение команды и направления
        auto dir = found->dll.dir;
        auto opcode = found->dll.opcode;
        // Подсчет общего количества байт
        size_t size = 0;
        for (auto slot = rx.used.head(); slot != NULL; slot = LIST_ITEM_NEXT(slot))
            if (slot->packet.equals(opcode, dir))
                size += slot->packet.dll.length;
        // Сборка
        auto skip = false;
        args_t args(size);
        for (auto s = rx.used.head(); s != NULL;)
        {
            // Ссылка на слот и пакет
            auto &slot = *s;
            const auto &packet = slot.packet;
            // Переход к следующему слоту
            s = LIST_ITEM_NEXT(s);
            // Фильтрация по команде и направлению
            if (!packet.equals(opcode, dir))
                continue;
            // Освобождение слота
            rx.free(slot);
            // Если пропуск
            if (skip)
                continue;
            // Обработка
            if (!receiver.packet_process(packet, args))
                skip = true;
            args.first = false;
        }
    }
}

bool ipc_link_t::packet_process(const ipc_packet_t &packet, const args_t &args)
{
    if (args.first)
    {
        // Проверяем, вместятся ли все данные
        auto slot_count = (args.size <= 0) ? 1 : DIV_CEIL(args.size, IPC_APL_SIZE);
        if (tx.unused.count() < slot_count)
            return false;
        // Проверяем, есть ли такая команда с таким же направлением
        for (auto slot = tx.used.head(); slot != NULL; slot = LIST_ITEM_NEXT(slot))
            if (slot->packet.equals(packet))
                return false;
    }
    assert(tx.unused.count() > 0);
    // Копирование пакета
    auto &slot = *tx.unused.head();
    slot.packet = packet;
    // Перенос в список используемых
    tx.use(slot);
    return true;
}

RAM bool ipc_link_master_t::check_phase(const ipc_packet_t &packet)
{
    if (ipc_link_t::check_phase(packet))
        // Фаза не слошлась, на другой стороне пропущен пакет
        retry.index = 0;
    return false;
}

void ipc_link_master_t::reset_layer(reset_reason_t reason, bool internal)
{
    retry.index = ARRAY_SIZE(retry.packet);
    // Базовый метод
    ipc_link_t::reset_layer(reason, internal);
    // Обработка счетчика ошибок передачи
    if (internal && reason == RESET_REASON_CORRUPTION)
        corruption_count += 4;
}

void ipc_link_master_t::packet_output(ipc_packet_t &packet)
{
    if (retry.index < ARRAY_SIZE(retry.packet))
    {
        // Переотправляем
        packet = retry.packet[retry.index++];
        return;
    }
    // Выводим и кэшируем
    ipc_link_t::packet_output(packet);
    retry.packet[0] = retry.packet[1];
    retry.packet[1] = packet;
}

void ipc_link_master_t::packet_input(const ipc_packet_t &packet)
{
    // Базовый метод
    ipc_link_t::packet_input(packet);
    // Обработка счетчика ошибок передачи
    if (corruption_count <= 0 || --corruption_count < 10)
        return;
    // Жопа
    corruption_count = 0;
    reset_slave();
}

RAM size_t ipc_command_t::buffer_size(ipc_dir_t dir) const
{
    // По умолчанию у команды данных нет
    return 0;
}

RAM const void * ipc_command_t::buffer_pointer(ipc_dir_t dir) const
{
    // По умолчанию у команды данных нет
    return NULL;
}

RAM size_t ipc_command_t::encode(ipc_dir_t dir)
{
    // Получает размер буфера
    return buffer_size(dir);
}

RAM bool ipc_command_t::decode(ipc_dir_t dir, size_t size)
{
    // По умолчанию размер полученных данных и буфер должены быть равены
    return buffer_size(dir) == size;
}

RAM bool ipc_command_t::transmit(ipc_processor_t &processor, ipc_dir_t dir)
{
    return ipc_processor_t::data_split(processor, opcode, dir, buffer_pointer(dir), encode(dir));
}

RAM ipc_handler_t * ipc_handler_host_t::find_handler(ipc_opcode_t opcode) const
{
    // Поиск обработчика с такой командой
    for (auto i = handlers.head(); i != NULL; i = LIST_ITEM_NEXT(i))
        if (i->command_get().opcode == opcode)
            return i;
    return NULL;
}

RAM void ipc_handler_host_t::pool(void)
{
    for (auto i = handlers.head(); i != NULL; i = LIST_ITEM_NEXT(i))
        i->pool();
}

void ipc_handler_host_t::add_handler(ipc_handler_t &handler)
{
    // Поиск обработчика с такой командой
    assert(find_handler(handler.command_get().opcode) == NULL);
    // Добавление
    handler.link(handlers);
}

bool ipc_handler_host_t::packet_process(const ipc_packet_t &packet, const args_t &args)
{
    if (args.first)
    {
        // Поиск обработчика по команде
        processing.handler = find_handler(packet.dll.opcode);
        // Сброс смещения записи
        processing.offset = 0;
    }
    assert(processing.handler != NULL);
    // Получаем направление и команду
    auto dir = packet.dll.dir;
    auto &command = processing.handler->command_get();
    // Получаем длинну пакета
    auto len = packet.dll.length;
    assert(len <= args.size);
    // Вместятся ли данные
    if (args.first && command.buffer_size(dir) < args.size)
        return false;
    // Запись в буффер
    auto buffer = (uint8_t *)command.buffer_pointer(dir);
    assert(buffer != NULL || args.size <= 0);
    memcpy(buffer + processing.offset, packet.apl.u8, len);
    // Смещение указателей
    processing.offset += len;
    if (packet.dll.more)
        return true;
    assert(processing.offset == args.size);
    // Декодирование данных, оповещение обработчика
    if (command.decode(dir, processing.offset))
        processing.handler->notify(dir);
    // Успех
    return true;
}

int16_t ipc_string_length(const char *s, size_t size)
{
    assert(s != NULL && size > 0);
    for (size_t i = 0; i < size; i++)
        if (s[i] == '\0')
            return (int16_t)i;
    return -1;
}
