// Данные ответа команды даты/времени
ALIGN_FIELD_8
struct command_datetime_sync_response_t
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
    command_datetime_sync_response_t(void) : quality(QUALITY_SUCCESS)
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

class command_datetime_sync_t : public ipc_command_data_template_getter_t<command_datetime_sync_response_t>
{
public:
    // Конструктор по умолчанию
    command_datetime_sync_t(void) : ipc_command_data_template_getter_t(IPC_COMMAND_STM_DATETIME_SYNC)
    { }
};
