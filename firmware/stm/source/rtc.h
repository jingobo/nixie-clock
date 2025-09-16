#ifndef __RTC_H
#define __RTC_H

#include <list.h>
#include <datetime.h>

// Частота кварца LSE по умолчанию
constexpr const uint16_t RTC_LSE_FREQ_DEFAULT = 32768;

// Локальное время (только чтение)
extern datetime_t rtc_time;
// День недели (только чтение)
extern uint8_t rtc_week_day;
// Частота кварца LSE (только чтение)
extern uint16_t rtc_lse_freq;
// Количество секунд с запуска (только чтение)
extern uint32_t rtc_uptime_seconds;

// Инициализация модуля
void rtc_init(void);
// Вывод частоты RTC /64
void rtc_clock_output(bool enabled);
// Добавление обработчика секундного события 
void rtc_second_event_add(list_handler_item_t &handler);

// Задает локальное время
void rtc_time_set(const datetime_t &value);
// Задает частота кварца LSE
void rtc_lse_freq_set(uint16_t value);

// Обработчик секундного прерывания
void rtc_interrupt_second(void);

#endif // __RTC_H
