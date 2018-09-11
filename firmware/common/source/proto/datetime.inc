ALIGN_FIELD_8
// Загрушка для структры запроса/ответа
struct command_common_dummy_t
{
    uint8_t dummy;
    // Конструктор по умолчанию
    command_common_dummy_t(void) : dummy(0)
    { }

    // Проверка значения
    ROM bool check(void) const
    {
        return dummy == 0;
    }
};

// Данные ответа команды даты/времени
struct command_datetime_response_t
{
    // Дата/время (возможно не валидная)
    datetime_t datetime;
    // Код качества
    enum quality_t
    {
        // Нормальное время
        QUALITY_SUCCESS = 0,
        // Нет списка серверов
        QUALITY_NOLIST,
        // Проблемы с сетью
        QUALITY_NETWORK,

        // Для определения количества значений
        QUALITY_COUNT
    } quality : 8;

    // Конструктор по умолчанию
    command_datetime_response_t(void) : quality(QUALITY_SUCCESS)
    { }

    // Провера полей
    ROM bool check(void) const
    {
        if (quality >= QUALITY_COUNT)
            return false;
        return quality != QUALITY_SUCCESS || datetime.check();
    }
};
ALIGN_FIELD_DEF

class command_datetime_t : public ipc_command_data_template_t<command_common_dummy_t, command_datetime_response_t>
{
public:
    // Конструктор по умолчанию
    command_datetime_t(void) : ipc_command_data_template_t(1) // 1 - код команды DATETIME
    { }
protected:
    // Кодирование данных, возвращает количество записанных данных
    virtual size_t encode(ipc_dir_t dir) const;
    // Декодирование данных
    virtual bool decode(ipc_dir_t dir, size_t size);
};

// Кодирование данных, возвращает количество записанных данных
ROM size_t command_datetime_t::encode(ipc_dir_t dir) const
{
    // Проверка данных
    assert(dir != IPC_DIR_RESPONSE || response.check());
    assert(dir != IPC_DIR_REQUEST || request.check());
    // Базовый метод
    return ipc_command_data_template_t::encode(dir);
}

// Декодирование данных
ROM bool command_datetime_t::decode(ipc_dir_t dir, size_t size)
{
    // Проверка данных
    switch (dir)
    {
        case IPC_DIR_REQUEST:
            if (!request.check())
                return false;
            break;
        case IPC_DIR_RESPONSE:
            if (!response.check())
                return false;
            break;
        default:
            return false;
    }
    // Базовый метод
    return ipc_command_data_template_t::decode(dir, size);
}

