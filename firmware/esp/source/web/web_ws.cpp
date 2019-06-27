// TODO: убрать
#define _CRT_SECURE_NO_WARNINGS
#include "web_ws.h"

void web_ws_handler_t::free(web_slot_free_reason_t reason)
{
    // Базовый метод
    web_slot_handler_t::free(reason);
    // Сброс
    reset();
}

void web_ws_handler_t::process_in(web_slot_buffer_t buffer, size_t readed)
{
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
                        socket->free(WEB_SLOT_FREE_REASON_NORMAL);
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
                // Если фрейм последний
                if (frame.in.header.control.fin)
                {
                    transmit(frame.in.header.control.code, frame.in.payload, frame.in.offset.payload);
                    // TODO: отправить полученные данные
                    frame.in.offset.payload = 0;
                }
                break;
        }
        // Проверка длинны полезных данных
        if (frame.in.offset.payload > WEB_WS_PAYLOAD_SIZE)
        {
            socket->free(WEB_SLOT_FREE_REASON_NORMAL);
            return;
        }
    }
}

bool web_ws_handler_t::transmit(uint8_t code, const void *source, size_t size)
{
    // Проверка аргументов
    assert(source != NULL && size <= WEB_WS_PAYLOAD_SIZE);
    // Если что то отправляем
    if (frame.out.remain > 0)
        return false;
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

bool web_ws_handler_t::execute(web_slot_buffer_t buffer)
{
    if (!web_slot_handler_t::execute(buffer))
        return false;
    // Обработка чтения
    auto transfered = socket->read(buffer);
    // Если соединение закрыто
    if (transfered == 0)
        return false;
    if (transfered > 0)
        process_in(buffer, (size_t)transfered);
    // Возможно сокет был закрыт
    if (!busy())
        return false;
    // Обработка записи
    if (frame.out.remain > 0)
    {
        transfered = socket->write(frame.out.buffer + frame.out.offset, (int)frame.out.remain);
        // Если соединение закрыто
        if (transfered == 0)
            return false;
        // Если что то передали
        if (transfered > 0)
        {
            auto written = (size_t)transfered;
            frame.out.remain -= written;
            frame.out.offset += written;
        }
    }
    return true;
}
