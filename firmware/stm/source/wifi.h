#ifndef __WIFI_H
#define __WIFI_H

// Инициализация модуля
void wifi_init(void);
// Получает есть ли теоретически интернет
bool wifi_has_internet_get(void);
// Оповещение о смене настроек WiFi
void wifi_settings_changed(void);

#endif // __WIFI_H
