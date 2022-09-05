#include <led.h>
#include <neon.h>
#include <nixie.h>

// Структура ответа команды получения состояния экрана
struct screen_command_state_get_response_t
{
    // Данные ламп
    nixie_data_t nixie[NIXIE_COUNT];

    // Данные светодиодов
    hmi_rgb_t led[LED_COUNT];

    // Данные неонок
    neon_data_t neon[NEON_COUNT];

    // Проверка полей
    bool check(void) const
    {
        for (auto i = 0; i < NIXIE_COUNT; i++)
        {
            if (nixie[i].digit > NIXIE_DIGIT_SPACE)
                return false;
            
            if (!ipc_bool_check(nixie[i].dot))
                return false;
        }
        
        return true;
    }
};

// Команда получения состояния экрана
class screen_command_state_get_t : public ipc_command_get_t<screen_command_state_get_response_t>
{
public:
    // Конструктор по умолчанию
    screen_command_state_get_t(void) : ipc_command_get_t(IPC_OPCODE_STM_SCREEN_STATE_GET)
    { }
};
