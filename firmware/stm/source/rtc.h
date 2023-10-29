#ifndef __RTC_H
#define __RTC_H

#include <list.h>
#include <datetime.h>

// Частота кварца LSE по умолчанию
constexpr const uint16_t RTC_LSE_FREQ_DEFAULT = 32768;

// Локальное время
extern datetime_t rtc_time;
// Количество секунд с запуска
extern uint32_t rtc_uptime_seconds;

// Инициализация модуля
void rtc_init(void);
// Вывод частоты RTC /64
void rtc_clock_output(bool enabled);
// Добавление обработчика секундного события 
void rtc_second_event_add(list_handler_item_t &handler);

// Получает/Задает частота кварца LSE
uint16_t rtc_lse_freq_get(void);
void rtc_lse_freq_set(uint16_t value);

// Обработчик секундного прерывания
void rtc_interrupt_second(void);

#endif // __RTC_H
