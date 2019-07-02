#include <sha1.h>
#include <base64.h>
#include <system.h>
#include "web_http.h"

// Отчищает строку
#define WEB_HTTP_STR_CLR(x)         (x)[0] = 0
// Рамер строки в символах
#define WEB_HTTP_STR_MAX(x)         (sizeof(x) - 1)
// Ожидаемая версия WebSocket
#define WEB_HTTP_WEBSOCKET_VERSION  13
// Симолы возрата и перевода строки
#define WEB_HTTP_CRLF               "\r\n"
// Строка для MIME типа с указанием кодировки UTF-8
#define WEB_HTTP_CHARSET_UTF8       "; charset=utf-8" WEB_HTTP_CRLF

// --- Общие символы --- //

static const char WEB_HTTP_SYM_LF = '\n';
static const char WEB_HTTP_SYM_SP = ':';
static const char WEB_HTTP_SYM_CR = '\r';
static const char WEB_HTTP_SYM_DOT = '.';
static const char WEB_HTTP_SYM_ZERO = '\0';
static const char WEB_HTTP_SYM_SPACE = ' ';

// --- Различные строки --- //

static const char WEB_HTTP_STR_CRLF[] = WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_ROOT[] = "/";
static const char WEB_HTTP_STR_INDEX[] = "index.html";
static const char WEB_HTTP_STR_METHOD_GET[] = "GET";
static const char WEB_HTTP_STR_PROTO_VERSION_0[] = "HTTP/1.0";
static const char WEB_HTTP_STR_PROTO_VERSION_1[] = "HTTP/1.1";
static const char WEB_HTTP_STR_CONTENT_LENGTH[] = "Content-Length: ";
static const char WEB_HTTP_STR_CONTENT_TYPE[] = "Content-Type: ";
static const char WEB_HTTP_STR_FRM_NUMBER_CRLF[] = "%d" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_WEBSOCKET_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// --- Строки HTTP статусов --- //

static const char WEB_HTTP_STR_STATUS_OK[] = " 200 OK" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_STATUS_BAD_REQUEST[] = " 400 Bad Request" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_STATUS_NOT_FOUND[] = " 404 Not Found" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_STATUS_METHOD_NOT_ALLOWED[] = " 405 Method Not Allowed" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_STATUS_URI_TOO_LONG[] = " 414 URI Too Long" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_STATUS_IM_A_TEAPOT[] = " 418 I’m a teapot" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_STATUS_UPGRADE_REQUIRED[] = " 426 Upgrade Required" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_STATUS_SWITCHING_PROTOCOLS[] = " 101 Switching Protocols" WEB_HTTP_CRLF;

// --- Строки расширения файлов --- //

static const char WEB_HTTP_STR_FILE_EXT_HTML[] = "html";
static const char WEB_HTTP_STR_FILE_EXT_CSS[] = "css";
static const char WEB_HTTP_STR_FILE_EXT_PNG[] = "png";
static const char WEB_HTTP_STR_FILE_EXT_GIF[] = "gif";
static const char WEB_HTTP_STR_FILE_EXT_JS[] = "js";
static const char WEB_HTTP_STR_FILE_EXT_ICO[] = "ico";
static const char WEB_HTTP_STR_FILE_EXT_JPG[] = "jpg";

// --- Строки MIME типов --- //

static const char WEB_HTTP_STR_CONTENT_TYPE_HTML[] = "text/html" WEB_HTTP_CHARSET_UTF8;
static const char WEB_HTTP_STR_CONTENT_TYPE_CSS[] = "text/css" WEB_HTTP_CHARSET_UTF8;
static const char WEB_HTTP_STR_CONTENT_TYPE_PNG[] = "image/png" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_CONTENT_TYPE_GIF[] = "image/gif" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_CONTENT_TYPE_JS[] = "application/javascript" WEB_HTTP_CHARSET_UTF8;
static const char WEB_HTTP_STR_CONTENT_TYPE_ICO[] = "image/x-icon" WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_CONTENT_TYPE_JPG[] = "image/jpeg" WEB_HTTP_CRLF;

// --- Строки HTTP заголовоков --- //

static const char WEB_HTTP_STR_HEADER_CONNECTION[] = "connection";
static const char WEB_HTTP_STR_HEADER_CONNECTION_CLOSED[] = "closed";
static const char WEB_HTTP_STR_HEADER_CONNECTION_UPGRADE[] = "upgrade";
static const char WEB_HTTP_STR_HEADER_UPGRADE[] = "upgrade";
static const char WEB_HTTP_STR_HEADER_UPGRADE_WEBSOCKET[] = "websocket";
static const char WEB_HTTP_STR_HEADER_WEBSOCKET_VERSION_LOWER[] = "sec-websocket-version";
static const char WEB_HTTP_STR_HEADER_WEBSOCKET_KEY[] = "sec-websocket-key";
static const char WEB_HTTP_STR_HEADER_WEBSOCKET_VERSION_NORAML[] = "Sec-WebSocket-Version: ";
static const char WEB_HTTP_STR_HEADER_WEBSOCKET_ACCEPT[] = "Sec-WebSocket-Accept: ";
static const char WEB_HTTP_STR_HEADER_FINAL_DEFAULT[] =
    "Server: NixieClock (ESP8266EX)" WEB_HTTP_CRLF
    "Pragma: no-cache" WEB_HTTP_CRLF
    "Cache-Control: no-cache" WEB_HTTP_CRLF
    "Connection : close" WEB_HTTP_CRLF WEB_HTTP_CRLF;
static const char WEB_HTTP_STR_HEADER_FINAL_UPGRADE[] =
    "Upgrade: WebSocket" WEB_HTTP_CRLF
    "Connection: Upgrade" WEB_HTTP_CRLF WEB_HTTP_CRLF;

// Используемые HTTP статусы
const web_http_handler_t::http_status_text_t web_http_handler_t::HTTP_STATUSES[] =
{
    { HTTP_STATUS_OK,                   WEB_HTTP_STR_STATUS_OK },
    { HTTP_STATUS_BAD_REQUEST,          WEB_HTTP_STR_STATUS_BAD_REQUEST },
    { HTTP_STATUS_NOT_FOUND,            WEB_HTTP_STR_STATUS_NOT_FOUND },
    { HTTP_STATUS_METHOD_NOT_ALLOWED,   WEB_HTTP_STR_STATUS_METHOD_NOT_ALLOWED },
    { HTTP_STATUS_URI_TOO_LONG,         WEB_HTTP_STR_STATUS_URI_TOO_LONG },
    { HTTP_STATUS_IM_A_TEAPOT,          WEB_HTTP_STR_STATUS_IM_A_TEAPOT },
    { HTTP_STATUS_UPGRADE_REQUIRED,     WEB_HTTP_STR_STATUS_UPGRADE_REQUIRED },
    { HTTP_STATUS_SWITCHING_PROTOCOLS,  WEB_HTTP_STR_STATUS_SWITCHING_PROTOCOLS },
};

// Поддерживаемые типы файлов
const web_http_handler_t::content_type_t web_http_handler_t::CONTENT_TYPES[] =
{
    { WEB_HTTP_STR_FILE_EXT_HTML,   WEB_HTTP_STR_CONTENT_TYPE_HTML },
    { WEB_HTTP_STR_FILE_EXT_CSS,    WEB_HTTP_STR_CONTENT_TYPE_CSS },
    { WEB_HTTP_STR_FILE_EXT_PNG,    WEB_HTTP_STR_CONTENT_TYPE_PNG },
    { WEB_HTTP_STR_FILE_EXT_GIF,    WEB_HTTP_STR_CONTENT_TYPE_GIF },
    { WEB_HTTP_STR_FILE_EXT_JS,     WEB_HTTP_STR_CONTENT_TYPE_JS },
    { WEB_HTTP_STR_FILE_EXT_ICO,    WEB_HTTP_STR_CONTENT_TYPE_ICO },
    { WEB_HTTP_STR_FILE_EXT_JPG,    WEB_HTTP_STR_CONTENT_TYPE_JPG },
};

// Для хэширования WebSocket рукопожатий
static sha1_t web_hhtp_sha1;

// --- Методы --- //

const char * web_http_handler_t::http_status_text(http_status_t code)
{
    for (auto i = 0; i < ARRAY_SIZE(HTTP_STATUSES); i++)
        if (HTTP_STATUSES[i].code == code)
            return HTTP_STATUSES[i].text;
    // WTF?
    assert(0);
    return NULL;
}

const char * web_http_handler_t::content_type_mime(const char *ext)
{
    for (auto i = 0; i < ARRAY_SIZE(CONTENT_TYPES); i++)
        if (!strcmp(ext, CONTENT_TYPES[i].ext))
            return CONTENT_TYPES[i].mime;
    return NULL;
}

bool web_http_handler_t::str_cat(char *dest, char c, size_t n)
{
    auto len = strlen(dest);
    if (len >= n)
        return true;
    dest[len++] = c;
    dest[len] = WEB_HTTP_SYM_ZERO;
    return false;
}

// Перевод символов строки в нижний регитстр
char * web_http_handler_t::str_lower(char *s)
{
    auto len = strlen(s);
    for (size_t i = 0; i < len; i++)
        if (s[i] >= 'A' && s[i] <= 'Z')
            s[i] += 32;
    return s;
}

bool web_http_handler_t::path_check(char c)
{
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))
        return false;
    switch (c)
    {
    case '.': case '-': case '_': case '~':
    case '!': case '$': case '&': case '\'':
    case '(': case ')': case '*': case '+':
    case ',': case ';': case ':': case '=':
    case '@': case '/': case '\\':
        return false;
    }
    return true;
}

void web_http_handler_t::request_t::clear(void)
{
    state = STATE_METHOD;
    WEB_HTTP_STR_CLR(headers.path);
    WEB_HTTP_STR_CLR(headers.method);
    WEB_HTTP_STR_CLR(headers.version);
    headers.websocket.version = 0;
    headers.upgrade = HTTP_HEADER_UPGRADE_UNKNOWN;
    headers.connection = HTTP_HEADER_CONNECTION_UNKNOWN;
    WEB_HTTP_STR_CLR(headers.websocket.key);
}

web_http_handler_t::http_status_t web_http_handler_t::request_t::process(const web_slot_buffer_t data, size_t size)
{
    for (size_t offset = 0; offset < size; offset++)
    {
        auto c = data[offset];
        switch (state)
        {
            case STATE_METHOD:
                if (c == WEB_HTTP_SYM_SPACE)
                {
                    if (strcmp(headers.method, WEB_HTTP_STR_METHOD_GET))
                        return HTTP_STATUS_METHOD_NOT_ALLOWED;
                    state = STATE_PATH;
                }
                else if (str_cat(headers.method, c, WEB_HTTP_STR_MAX(headers.method)))
                    return HTTP_STATUS_METHOD_NOT_ALLOWED;
                break;
            case STATE_PATH:
                if (c == WEB_HTTP_SYM_SPACE)
                {
                    if (strlen(headers.path) <= 0)
                        return HTTP_STATUS_BAD_REQUEST;
                    state = STATE_VERSION;
                }
                else if (path_check(c) || str_cat(headers.path, c, WEB_HTTP_STR_MAX(headers.path)))
                    return HTTP_STATUS_URI_TOO_LONG;
                break;
            case STATE_VERSION:
                if (c == WEB_HTTP_SYM_CR)
                {
                    if (strcmp(headers.version, WEB_HTTP_STR_PROTO_VERSION_1) &&
                        strcmp(headers.version, WEB_HTTP_STR_PROTO_VERSION_0))
                        return HTTP_STATUS_BAD_REQUEST;
                    end_detect = 1;
                    state = STATE_END_LF;
                }
                else if (str_cat(headers.version, c, WEB_HTTP_STR_MAX(headers.version)))
                    return HTTP_STATUS_BAD_REQUEST;
                break;

            case STATE_END_HN:
                // Если возврат каретки
                if (c == WEB_HTTP_SYM_CR)
                {
                    // Если имя уже не пустое - значит что то не так
                    if (strlen(headers.temp.name) > 0)
                        return HTTP_STATUS_BAD_REQUEST;
                    state = STATE_END_LF;
                    break;
                }
                end_detect++;
                // Пробел не интересует
                if (c == WEB_HTTP_SYM_SPACE)
                    break;
                // Если разделитель - конец имени
                if (c == WEB_HTTP_SYM_SP)
                {
                    str_lower(headers.temp.name);
                    // Переход к загрузке значения
                    WEB_HTTP_STR_CLR(headers.temp.value);
                    state = STATE_END_HV;
                    break;
                }
                // Символ перенос строки не допустим
                if (c == WEB_HTTP_SYM_LF)
                    return HTTP_STATUS_BAD_REQUEST;
                // Конкатезация
                if (str_cat(headers.temp.name, c, WEB_HTTP_STR_MAX(headers.temp.name)))
                {
                    // Если переполнение - не считаем ошибкой, значит этот заголовок нас не интересует
                    state = STATE_END_CR;
                    break;
                }
                break;

            case STATE_END_HV:
                // Пробел не интересует
                if (c == WEB_HTTP_SYM_SPACE)
                    break;
                // Символ перенос строки не допустим
                if (c == WEB_HTTP_SYM_LF)
                    return HTTP_STATUS_BAD_REQUEST;
                // Если возврат каретки
                if (c == WEB_HTTP_SYM_CR)
                {
                    // Определяем что нам прислали
                    if (!strcmp(headers.temp.name, WEB_HTTP_STR_HEADER_CONNECTION))
                    {
                        str_lower(headers.temp.value);
                        if (!strcmp(headers.temp.value, WEB_HTTP_STR_HEADER_CONNECTION_CLOSED))
                            headers.connection = HTTP_HEADER_CONNECTION_CLOSED;
                        else if (!strcmp(headers.temp.value, WEB_HTTP_STR_HEADER_CONNECTION_UPGRADE))
                            headers.connection = HTTP_HEADER_CONNECTION_UPGRADE;
                    }
                    else if (!strcmp(headers.temp.name, WEB_HTTP_STR_HEADER_UPGRADE))
                    {
                        str_lower(headers.temp.value);
                        if (!strcmp(headers.temp.value, WEB_HTTP_STR_HEADER_UPGRADE_WEBSOCKET))
                            headers.upgrade = HTTP_HEADER_UPGRADE_WEBSOCKET;
                    }
                    else if (!strcmp(headers.temp.name, WEB_HTTP_STR_HEADER_WEBSOCKET_VERSION_LOWER))
                        sscanf(headers.temp.value, "%d", &headers.websocket.version);
                    else if (!strcmp(headers.temp.name, WEB_HTTP_STR_HEADER_WEBSOCKET_KEY))
                    {
                        // Проверка длинны
                        if (strlen(headers.temp.value) >= sizeof(headers.websocket.key))
                            return HTTP_STATUS_BAD_REQUEST;
                        strcpy(headers.websocket.key, headers.temp.value);
                    }
                    // Остальное не интересует, переход к переводу строки
                    state = STATE_END_LF;
                    break;
                }
                // Конкатезация
                if (str_cat(headers.temp.value, c, WEB_HTTP_STR_MAX(headers.temp.value)))
                {
                    // Если переполнение - не считаем ошибкой, значит это значение нас не интересует
                    state = STATE_END_CR;
                    break;
                }
                break;

            case STATE_END_CR:
                if (c == WEB_HTTP_SYM_CR)
                    state = STATE_END_LF;
                else
                    end_detect++;
                break;
            case STATE_END_LF:
                if (c != WEB_HTTP_SYM_LF)
                    return HTTP_STATUS_BAD_REQUEST;
                if (!end_detect)
                    return HTTP_STATUS_OK;
                // Переход к значению заголовка
                WEB_HTTP_STR_CLR(headers.temp.name);
                state = STATE_END_HN;
                end_detect = 0;
                break;
        }
    }
    return HTTP_STATUS_NA;
}

void web_http_handler_t::response_t::clear(void)
{
    total = 0;
    mime = NULL;
    file = fs_file_t();
    state = STATE_INITIAL;
    header.dynamic.name = NULL;
    header.dynamic.value = NULL;
    header.final = WEB_HTTP_STR_HEADER_FINAL_DEFAULT;
}

size_t web_http_handler_t::response_t::send_str(web_slot_buffer_t dest, const char *source, size_t size)
{
    // Проверка аргументов
    assert(dest != NULL && source != NULL);
    // Если смещения нет - указываем сколько данных всего
    if (offset <= 0)
        total = size;
    // Определение количества передаваемых байт
    size -= offset;
    size = MIN(sizeof(web_slot_buffer_t), size);
    // Копирование части ответа
    memcpy(dest, source, size);
    return size;
}

// Обработка передачи
size_t web_http_handler_t::response_t::process(web_slot_buffer_t dest)
{
    // Если все данные переданы...
    if (total <= 0)
    {
        // Переход к следующему состоянию
        offset = 0;
        state = (state_t)(state + 1);
    }
    // Относительно состояния передача данных в исходящий буфер
    for (;;)
        switch (state)
        {
            case STATE_VERSION:
                return send_str(dest, WEB_HTTP_STR_PROTO_VERSION_1, strlen(WEB_HTTP_STR_PROTO_VERSION_1));
            case STATE_STATUS:
            {
                auto text = http_status_text(status);
                return send_str(dest, text, strlen(text));
            }

            // --- Тип контента --- //
            case STATE_CONTENT_TYPE_HEAD:
                if (mime == NULL)
                {
                    state = STATE_CONTENT_LENGTH_HEAD;
                    continue;
                }
                return send_str(dest, WEB_HTTP_STR_CONTENT_TYPE, strlen(WEB_HTTP_STR_CONTENT_TYPE));
                break;
            case STATE_CONTENT_TYPE_BODY:
                assert(mime != NULL);
                return send_str(dest, mime, strlen(mime));

            // --- Длинна контента --- //
            case STATE_CONTENT_LENGTH_HEAD:
                if (!file.opened())
                {
                    state = STATE_DYNAMIC_HEADER_NAME;
                    continue;
                }
                return send_str(dest, WEB_HTTP_STR_CONTENT_LENGTH, strlen(WEB_HTTP_STR_CONTENT_LENGTH));
            case STATE_CONTENT_LENGTH_BODY:
                {
                    char buf[9]; // До 999999 байт
                    auto size = file.size();
                    assert(size <= 9999999);
                    sprintf(buf, WEB_HTTP_STR_FRM_NUMBER_CRLF, size);
                    return send_str(dest, buf, strlen(buf));
                }

            // --- Динамический заголовок --- //
            case STATE_DYNAMIC_HEADER_NAME:
                if (header.dynamic.name == NULL)
                {
                    state = STATE_STATIC_HEADER;
                    continue;
                }
                assert(header.dynamic.value != NULL);
                return send_str(dest, header.dynamic.name, strlen(header.dynamic.name));
            case STATE_DYNAMIC_HEADER_VALUE:
                assert(header.dynamic.value != NULL);
                return send_str(dest, header.dynamic.value, strlen(header.dynamic.value));

            // --- Статический заголовок --- //
            case STATE_STATIC_HEADER:
                assert(header.final != NULL);
                return send_str(dest, header.final, strlen(header.final));

            // --- Файловый контент --- //
            case STATE_CONTENT:
                // Если смещения нет - указываем сколько данных всего
                if (offset <= 0)
                    total = file.size();
                else if (!file.seek(offset))
                    return 0;
                // Определение количества передаваемых байт
                {
                    auto size = MIN(sizeof(web_slot_buffer_t), total);
                    // Копирование части ответа
                    return file.read(dest, size) ? size : 0;
                }
            default:
                return 0;
        }
}

void web_http_handler_t::clear(void)
{
    request.clear();
    response.clear();
    responsing = false;
}

void web_http_handler_t::free(web_slot_free_reason_t reason)
{
    // Базовый ментод
    web_slot_handler_t::free(reason);
    // Закрытие файла
    response.file.close();
    // Отчистка полей
    clear();
}

void web_http_handler_t::process(web_slot_buffer_t buffer)
{
    // Проверяем на WebSocket
    if (request.headers.connection == HTTP_HEADER_CONNECTION_UPGRADE)
    {
        // Если обновляемся не до WebSocket
        if (request.headers.upgrade != HTTP_HEADER_UPGRADE_WEBSOCKET)
        {
            response.status = HTTP_STATUS_IM_A_TEAPOT;
            return;
        }
        // В любом случае будет динамический заголовок
        response.header.dynamic.value = request.headers.temp.value;
        // Если не известная версия WebSocket
        if (request.headers.websocket.version != WEB_HTTP_WEBSOCKET_VERSION)
        {
            response.status = HTTP_STATUS_UPGRADE_REQUIRED;
            response.header.dynamic.name = WEB_HTTP_STR_HEADER_WEBSOCKET_VERSION_NORAML;
            sprintf(request.headers.temp.value, WEB_HTTP_STR_FRM_NUMBER_CRLF, WEB_HTTP_WEBSOCKET_VERSION);
            return;
        }
        // Готовим ответ на переключение протокола
        response.status = HTTP_STATUS_SWITCHING_PROTOCOLS;
        response.header.final = WEB_HTTP_STR_HEADER_FINAL_UPGRADE;
        response.header.dynamic.name = WEB_HTTP_STR_HEADER_WEBSOCKET_ACCEPT;
        // Готовим ключ
        web_hhtp_sha1.reset();
        web_hhtp_sha1.update(request.headers.websocket.key, strlen(request.headers.websocket.key));
        web_hhtp_sha1.update(WEB_HTTP_STR_WEBSOCKET_GUID, strlen(WEB_HTTP_STR_WEBSOCKET_GUID));
        // В base64
        base64_encode(request.headers.temp.value, web_hhtp_sha1.final(), SHA1_HASH_SIZE);
        strcat(request.headers.temp.value, WEB_HTTP_STR_CRLF);
        socket->log("WebSocket handshake %s", request.headers.temp.value);
        return;
    }
    // Проверяем на индекс
    if (!strcmp(request.headers.path, WEB_HTTP_STR_ROOT))
        strcat(request.headers.path, WEB_HTTP_STR_INDEX);
    socket->log("Request %s", request.headers.path);
    // Определяем расширение файла
    size_t found = 0;
    auto size = strlen(request.headers.path);
    for (auto i = size - 1; i > 0; i--)
        if (request.headers.path[i] == WEB_HTTP_SYM_DOT)
        {
            found = i + 1;
            break;
        }
    // Если не удвлось определить 
    if (found <= 0)
    {
        socket->log("Unknown file extension!");
        response.status = HTTP_STATUS_NOT_FOUND;
        return;
    }
    auto ext = request.headers.path + found;
    socket->log("File extension is %s", ext);
    response.mime = content_type_mime(ext);
    // Если MIME тип не определен
    if (response.mime == NULL)
    {
        socket->log("Unknown MIME type!");
        response.status = HTTP_STATUS_NOT_FOUND;
        return;
    }
    // ...Пробуем открыть файл
    response.file = fs_open(request.headers.path);
    if (!response.file.opened())
    {
        socket->log("File not found!");
        response.status = HTTP_STATUS_NOT_FOUND;
        response.mime = WEB_HTTP_STR_CONTENT_TYPE_HTML;
    }
}

void web_http_handler_t::execute(web_slot_buffer_t buffer)
{
    // Чтение
    auto size = socket->read(buffer);
    // Если соединение закрыто
    if (size < 0)
        return;
    // Стадия запроса
    if (!responsing)
    {
        if (size == 0)
            // Ничего не получено
            return;
        // Обработка заголовков запроса
        response.status = request.process(buffer, (size_t)size);
        if (response.status == HTTP_STATUS_NA)
            // Не весь запрос получен
            return;
        // Обработка запроса
        responsing = true;
        // Если нет ошибки
        if (response.status == HTTP_STATUS_OK)
            process(buffer);
    }
    // Стадия отправки ответа
    size = response.process(buffer);
    if (size <= 0)
    {
        // Передача рукопожатия WebSocket завершена
        if (response.status == HTTP_STATUS_SWITCHING_PROTOCOLS)
        {
            do
            {
                // Есть ли аллокатор
                if (ws == NULL)
                    break;
                // Выделение слота
                auto ws_slot = ws->allocate(*socket);
                // Переход на новый обработчик
                if (ws_slot != NULL)
                {
                    socket->handler_change(ws_slot, WEB_SLOT_FREE_REASON_MIGRATION);
                    return;
                }
            } while (false);
            socket->log("No free WebSocket slots!");
        }
        // Больше отправлять не нужно
        socket->free(WEB_SLOT_FREE_REASON_NORMAL);
        return;
    }
    // Передача
    size = socket->write(buffer, size);
    // Если соединение закрыто или ничего не передали
    if (size <= 0)
        return;
    // Уведомление о количестве переданных байт
    response.process_feedback((size_t)size);
}

bool web_http_handler_t::allocate(web_slot_socket_t &socket)
{
    auto result = web_slot_handler_t::allocate(socket);
    if (result)
        socket.timeout_change(2000);
    return result;
}
