#ifndef __WEB_HTTP_H
#define __WEB_HTTP_H

#include "web_slot.h"
#include <romfs.h>

// Дескриптор файла TODO: перенести куда нить
#define FILE                romfs_t::reader_t::handle_t

// Обработчик HTTP запросов
class web_http_handler_t : public web_slot_handler_t
{
    // HTTP коды статуса
    enum http_status_t
    {
        // Без значения
        HTTP_STATUS_NA = 0,
        // Нормальный ответ
        HTTP_STATUS_SWITCHING_PROTOCOLS = 101,
        // Нормальный ответ
        HTTP_STATUS_OK = 200,
        // Не верный запрос
        HTTP_STATUS_BAD_REQUEST = 400,
        // Ресурс не найден
        HTTP_STATUS_NOT_FOUND = 404,
        // Метод не поддержкивается
        HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
        // Запрашиваемый путь слишком длинный
        HTTP_STATUS_URI_TOO_LONG = 414,
        // Я чайник
        HTTP_STATUS_IM_A_TEAPOT = 418,
        // Необходимость обновить протокол
        HTTP_STATUS_UPGRADE_REQUIRED = 426,
    };

    // Возможные значения типа соединения в заголовках
    enum http_header_connection_t
    {
        // Неизвестный тип соединения
        HTTP_HEADER_CONNECTION_UNKNOWN,
        // Переход на другой протокол
        HTTP_HEADER_CONNECTION_UPGRADE,
        // Соедиение закрыто
        HTTP_HEADER_CONNECTION_CLOSED,
    };

    // Возможные значения для типа нового протокола
    enum http_header_upgrade_t
    {
        // Протокол не известен
        HTTP_HEADER_UPGRADE_UNKNOWN,
        // Протокол WebSocket
        HTTP_HEADER_UPGRADE_WEBSOCKET,
    };

    // Флаг, указывающий, что происходит ответа
    bool responsing;
    // Аллокатор WebSocket
    web_slot_handler_allocator_t *ws;
    // Данные запроса
    class request_t
    {
        // Состояние парсера
        enum
        {
            STATE_METHOD,
            STATE_PATH,
            STATE_VERSION,
            STATE_END_CR,
            STATE_END_LF,
            STATE_END_HN,
            STATE_END_HV,
        } state;
        // Детектер конца запроса
        size_t end_detect;
    public:
        // Получаенные данных заголовков
        struct
        {
            // Метод
            char method[4];
            // Запрашиваемый путь
            char path[16];
            // Версия HTTP потокола
            char version[10];
            // Значение заголовка нового протокола
            http_header_upgrade_t upgrade;
            // Значение заголовка соединения
            http_header_connection_t connection;
            // Заголовки WebSocket
            struct
            {
                // Версия
                int version;
                // Ключ
                char key[26];
            } websocket;
            // Для данных текущего загружаемого заголовка
            struct
            {
                // Имя
                char name[22];
                // Значение
                char value[32];
            } temp;
        } headers;

        // Сброс полей
        void clear(void);

        // Обработка запроса
        http_status_t process(const web_slot_buffer_t data, size_t size);
    } request;
    // Данные ответа
    class response_t
    {
        // Смещение и общий размер передаваемых данных
        size_t offset, total;
        // Текущее состояние передачи
        enum state_t
        {
            // Начальное состояние
            STATE_INITIAL,
            // Версия протокола
            STATE_VERSION,
            // Статус ответа
            STATE_STATUS,
            // Размер тела ответа (заголовок)
            STATE_CONTENT_LENGTH_HEAD,
            // Размер тела ответа (значение)
            STATE_CONTENT_LENGTH_BODY,
            // Тип контента (заголовок)
            STATE_CONTENT_TYPE_HEAD,
            // Тип контента (значение)
            STATE_CONTENT_TYPE_BODY,
            // Имя динамического заголовка
            STATE_DYNAMIC_HEADER_NAME,
            // Значение динамического заголовка
            STATE_DYNAMIC_HEADER_VALUE,
            // Статический заголовок
            STATE_STATIC_HEADER,
            // Тело ответа
            STATE_CONTENT
        } state;

        // Передача данных в буфер
        size_t send_str(web_slot_buffer_t dest, const char *source, size_t size);
    public:
        // Текущий статус
        http_status_t status;
        // Передаваемый файл
        FILE file;
        // MIME тип открытого файла
        const char *mime;
        // Добавочные заголовки
        struct
        {
            // Динамический
            struct
            {
                // Имя заголовка с разделителем
                const char *name;
                // Значение с переводом строки
                const char *value;
            } dynamic;
            // Завершающий
            const char *final;
        } header;

        // Сброс автомата
        void clear(void);

        // Обработка передачи
        size_t process(web_slot_buffer_t dest);

        // Обратная связь передачи данных
        void process_feedback(size_t sended)
        {
            offset += sended;
            total -= sended;
        }
    } response;

    // Отчистка полей
    void clear(void);
    // Обработка запроса, инициализация ответа
    void process(web_slot_buffer_t buffer);

    // Используемые HTTP статусы
    static const struct http_status_text_t
    {
        // Код статуса
        http_status_t code;
        // Текст статуса
        const char * text;
    } HTTP_STATUSES[];

    // Поддерживаемые типы файлов
    static const struct content_type_t
    {
        // Расширение файла
        const char * ext;
        // MIME тип
        const char * mime;
    } CONTENT_TYPES[];

    // Получает текста статуса HTTP по коду
    static const char * http_status_text(http_status_t code);

    // Получает MIME тип по расширению файла
    static const char * content_type_mime(const char *ext);

    // Проверка символа пути, результат инверсный
    static bool path_check(char c);

    // Перевод символов строки в нижний регитстр
    static char * str_lower(char *s);
        
    // Конкатезация символа с проверкой переполнения, результат инверсный
    static bool str_cat(char *dest, char c, size_t n);
protected:
    // Освобождение обработчика
    virtual void free(web_slot_free_reason_t reason);

    // Обработка данных
    virtual bool execute(web_slot_buffer_t buffer);
public:
    // Конструктор по умолчанию
    web_http_handler_t(void) : ws(NULL)
    {
        clear();
    }
    // Конструктор с указанием аллокатора WebSocket
    web_http_handler_t(web_slot_handler_allocator_t &ws) : ws(&ws)
    {
        clear();
    }
};

// Шаблон аллокатора слотов HTTP обработчиков
template <int COUNT>
class web_http_handler_allocator_template_t : public web_slot_handler_allocator_template_t<web_http_handler_t, COUNT>
{
public:
    // Конструктор по умолчанию
    web_http_handler_allocator_template_t(web_slot_handler_allocator_t &ws_allocator)
    {
        for (auto i = 0; i < COUNT; i++)
            this->handlers[i] = web_http_handler_t(ws_allocator);
    }
};

#endif // __WEB_HTTP_H
