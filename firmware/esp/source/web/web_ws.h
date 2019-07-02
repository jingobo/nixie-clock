#ifndef __WEB_WS_H
#define __WEB_WS_H

#include "web_slot.h"
#include <stddef.h>
#include <stdint.h>

// Размер управляющего заголовка
#define WEB_WS_HEADER_CONTROL_SIZE          2
// Размер поля расширенной длинны
#define WEB_WS_HEADER_EXTEND_LENGTH_SIZE    2
// Размер поля маски
#define WEB_WS_HEADER_EXTEND_MASK_SIZE      4
// Размер расширенного заголовка
#define WEB_WS_HEADER_EXTEND_SIZE           (WEB_WS_HEADER_EXTEND_LENGTH_SIZE + WEB_WS_HEADER_EXTEND_MASK_SIZE)
// Размер полезных данных
#define WEB_WS_PAYLOAD_SIZE                 256

// Обработчик WebSocket
class web_ws_handler_t : public web_slot_handler_t
{
    // Состояние чтения/записи данных
    enum state_t
    {
        STATE_CONTROL,
        STATE_LENGTH,
        STATE_MASK,
        STATE_DATA,
    };

    // Шаблон фрейма
    template <int EXTSIZE>
    struct frame_t
    {
        union
        {
            struct
            {
                // Заголовок
                struct
                {
                    // Управляющий заголовок
                    union
                    {
                        struct
                        {
                            // 1 байт
                            uint8_t code : 4;
                            uint8_t rsv : 3;
                            uint8_t fin : 1;
                            // 2 байт
                            uint8_t length : 7;
                            uint8_t mask : 1;
                        };
                        // Сырое значение
                        uint16_t raw;
                    } control;
                    // Расширенный заголовок
                    uint8_t extend[EXTSIZE];
                } header;
                // Данные
                uint8_t payload[WEB_WS_PAYLOAD_SIZE];
            };
            // Сырые данные
            uint8_t buffer[WEB_WS_HEADER_CONTROL_SIZE + EXTSIZE + WEB_WS_PAYLOAD_SIZE];
        };
    };
    // Входящий фрейм
    struct frame_in_t : frame_t<WEB_WS_HEADER_EXTEND_SIZE>
    {
        // Смещения
        struct
        {
            // Состояния
            size_t state;
            // Полезных данных
            size_t payload;
        } offset;
        // Ожидаемых байт
        size_t remain;
        // Текущее состояние
        state_t state;

        // Сброс состояний
        void reset(void)
        {
            offset.payload = 0;
            state_set(STATE_CONTROL);
        }

        // Принятие решения перехода к маске или данным
        void mask_or_data(size_t o = 0)
        {
            if (header.control.mask)
            {
                state_set(STATE_MASK, o);
                return;
            }
            memset(header.extend + WEB_WS_HEADER_EXTEND_LENGTH_SIZE, 0, WEB_WS_HEADER_EXTEND_MASK_SIZE);
            state_set(STATE_DATA, o + WEB_WS_HEADER_EXTEND_MASK_SIZE);
        }

        // Установка нового состояния
        void state_set(state_t value, size_t o = 0)
        {
            state = value;
            switch (value)
            {
            case STATE_CONTROL:
                remain = WEB_WS_HEADER_CONTROL_SIZE;
                offset.state = 0;
                break;
            case STATE_LENGTH:
                remain = WEB_WS_HEADER_EXTEND_LENGTH_SIZE;
                break;
            case STATE_MASK:
                remain = WEB_WS_HEADER_EXTEND_MASK_SIZE;
                offset.state += o;
                break;
            case STATE_DATA:
                // Получаем размер данных (Big-Endian)
                remain = header.extend[0];
                remain <<= 8;
                remain |= header.extend[1];
                // Смещение буфера и размер данных
                offset.state += offset.payload + o;
                offset.payload += remain;
                break;
            default:
                assert(false);
                break;
            }
        }
    };
    // Исходящий фрейм
    struct frame_out_t : frame_t<WEB_WS_HEADER_EXTEND_LENGTH_SIZE>
    {
        // Смещение передаваемых данных
        size_t offset;
        // Количество оставшихся данных к передаче
        size_t remain;

        // Сброс состояния
        void reset(void)
        {
            remain = 0;
        }

        // Конструктор по умолчанию
        frame_out_t(void)
        {
            header.control.raw = 0;
            header.control.fin = 1;
            header.control.mask = 0;
            header.control.rsv = 0;
        }
    };

    // Фреймы
    struct
    {
        // Входящий
        frame_in_t in;
        // Исходящий
        frame_out_t out;
    } frame;

    // Сброс состояний
    void reset(void)
    {
        frame.in.reset();
        frame.out.reset();
    }

    // Обработка входящих данных
    void process_in(web_slot_buffer_t buffer, size_t size);
    // Передача данных
    bool transmit(uint8_t code, const void *source, size_t size);
protected:
    // Освобождение обработчика
    virtual void free(web_slot_free_reason_t reason);

    // Обработка данных
    virtual void execute(web_slot_buffer_t buffer);
public:
    // Конструктор по умолчанию
    web_ws_handler_t(void)
    {
        reset();
    }

};

// Шаблон аллокатора слотов WebSocket обработчиков
template <int COUNT>
class web_ws_handler_allocator_template_t : public web_slot_handler_allocator_template_t<web_ws_handler_t, COUNT>
{ };

#endif // __WEB_WS_H
