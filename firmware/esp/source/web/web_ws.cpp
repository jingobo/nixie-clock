#include "web_ws.h"

void web_ws_handler_t::free(web_slot_free_reason_t reason)
{
    // Базовый метод
    web_slot_handler_t::free(reason);
    // Сброс
    reset();
}

void web_ws_handler_t::process_in(web_slot_buffer_t buffer)
{
    assert(busy());
    // Обработка чтения
    auto transfered = socket->read(buffer);
    // Если соединение закрыто или ничего не получили
    if (transfered <= 0)
        return;
    auto readed = (size_t)transfered;
    for (size_t offset = 0; readed > 0;)
    {
        // Определяем сколько нужно прочитать
        auto r = frame.in.remain;
        if (r > readed)
            r = readed;
        // Чтение во входящий буфер
        memcpy(frame.in.buffer + frame.in.offset.state, buffer + offset, r);
        // Смещение
        offset += r;
        readed -= r;
        frame.in.remain -= r;
        frame.in.offset.state += r;
        // Если состояние еще не завершилось
        if (frame.in.remain > 0)
            continue;
        switch (frame.in.state)
        {
            case STATE_CONTROL:
                switch (frame.in.header.control.length)
                {
                    case 126:
                        // 16 бит
                        frame.in.state_set(STATE_LENGTH);
                        break;
                    case 127:
                        // 64 бит
                        socket->free(WEB_SLOT_FREE_REASON_INSIDE);
                        return;
                    default:
                        // 7 бит
                        frame.in.header.extend[0] = 0;
                        frame.in.header.extend[1] = frame.in.header.control.length;
                        // Переход к данным или маске
                        frame.in.mask_or_data(WEB_WS_HEADER_EXTEND_LENGTH_SIZE);
                        break;
                }
                break;
            case STATE_LENGTH:
                // Переход к данным или маске
                frame.in.mask_or_data(0);
                break;
            case STATE_MASK:
                // Переход к данным
                frame.in.state_set(STATE_DATA);
                break;
            case STATE_DATA:
                // XOR
                for (r = 0; r < frame.in.offset.payload; r++)
                    frame.in.payload[r] ^= frame.in.header.extend[WEB_WS_HEADER_EXTEND_LENGTH_SIZE + r % WEB_WS_HEADER_EXTEND_MASK_SIZE];
                // Сброс автомата
                frame.in.state_set(STATE_CONTROL);
                // Если фрейм не последний
                if (!frame.in.header.control.fin)
                    break;
                // Анализ опкода
                switch (frame.in.header.control.code)
                {
                    case WEB_WS_OPCODE_TEXT:
                        // Текстовые фреймы не обрабатываем
                        socket->log("Received text frame...skip");
                        break;
                    case WEB_WS_OPCODE_BINARY:
                        receive_event(frame.in.payload, frame.in.offset.payload);
                        break;
                    case WEB_WS_OPCODE_CLOSE:
                        // Нормальное закрытие
                        socket->timeout_change(1000);
                        socket->free(WEB_SLOT_FREE_REASON_OUTSIDE);
                        return;
                    case WEB_WS_OPCODE_PING:
                        // Пинг, отправляем ответ
                        transmit(WEB_WS_OPCODE_PONG, frame.in.payload, frame.in.offset.payload);
                        socket->log("Received ping frame");
                        break;
                    default:
                        socket->log("Received unknown frame opcode: %d!", frame.in.header.control.code);
                        break;
                }
                frame.in.offset.payload = 0;
                break;
        }
        // Проверка длинны полезных данных
        if (frame.in.offset.payload > WEB_WS_PAYLOAD_SIZE)
        {
            socket->free(WEB_SLOT_FREE_REASON_INSIDE);
            return;
        }
    }
}

void web_ws_handler_t::process_out(web_slot_buffer_t buffer)
{
    // Обработка записи
    if (!busy())
        return;
    // Если все данные отправлены
    if (frame.out.remain <= 0)
    {
        transmit_event();
        if (frame.out.remain <= 0)
            return;
    }
    auto transfered = socket->write(frame.out.buffer + frame.out.offset, (int)frame.out.remain);
    // Если соединение закрыто или ничего не передали
    if (transfered <= 0)
        return;
    // Если что то передали
    auto written = (size_t)transfered;
    frame.out.remain -= written;
    frame.out.offset += written;
}

bool web_ws_handler_t::transmit(uint8_t code, const void *source, size_t size)
{
    // Проверка аргументов
    assert(source != NULL && size <= WEB_WS_PAYLOAD_SIZE);
    // Если что то отправляем
    if (frame.out.remain > 0)
    {
        socket->log("Already transmitted!");
        return false;
    }
    // Формирование фрейма
    auto dest = frame.out.buffer + WEB_WS_HEADER_CONTROL_SIZE;
    frame.out.header.control.code = code;
    frame.out.remain = size + WEB_WS_HEADER_CONTROL_SIZE;
    if (size < 126)
        frame.out.header.control.length = (uint8_t)size;
    else
    {
        dest += WEB_WS_HEADER_EXTEND_LENGTH_SIZE;
        frame.out.remain += WEB_WS_HEADER_EXTEND_LENGTH_SIZE;
        // Расширенная длинна
        frame.out.header.control.length = 126;
        frame.out.header.extend[0] = (uint8_t)(size >> 8);
        frame.out.header.extend[1] = (uint8_t)(size >> 0);
    }
    // Данные
    memcpy(dest, source, size);
    // Смещения
    frame.out.offset = 0;
    return true;
}

void web_ws_handler_t::execute(web_slot_buffer_t buffer)
{
    process_in(buffer);
    process_out(buffer);
}

bool web_ws_handler_t::allocate(web_slot_socket_t &socket)
{
    auto result = web_slot_handler_t::allocate(socket);
    if (result)
        socket.timeout_change(1000 * 60 * 60); // 1 час
    return result;
}
