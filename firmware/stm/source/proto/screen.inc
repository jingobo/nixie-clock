#include <led.h>
#include <neon.h>
#include <nixie.h>

// Структура ответа команды получения состояния экрана
ALIGN_FIELD_8
struct screen_command_state_get_response_t
{
    // Данные ламп
    struct
    {
        // Цифра
        uint8_t digit;
        // Насыщенность
        uint8_t sat;
        // Точка
        ipc_bool_t dot : 8;
    } nixie[NIXIE_COUNT];

    // Данные светодиодов
    struct
    {
        // Красная компонента
        uint8_t r;
        // Зеленая компонента
        uint8_t g;
        // Синяя компонента
        uint8_t b;
    } led[LED_COUNT];

    // Данные неонок
    struct
    {
        // Насыщенность
        uint8_t sat;
    } neon[NEON_COUNT];

    // Проверка полей
    bool check(void) const
    {
        for (auto i = 0; i < NIXIE_COUNT; i++)
        {
            if (nixie[i].digit > NIXIE_DIGIT_SPACE)
                return false;
                
            if (nixie[i].dot > IPC_BOOL_TRUE)
                return false;
        }
        
        return true;
    }
};
ALIGN_FIELD_DEF

// Команда получения состояния экрана
class screen_command_state_get_t : public ipc_command_get_t<screen_command_state_get_response_t>
{
public:
    // Конструктор по умолчанию
    screen_command_state_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_SCREEN_STATE_GET)
    { }
};
