[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/c843050f193b43f386815034419c3617)](https://www.codacy.com/app/exclude/nixie-clock?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=jingobo/nixie-clock&amp;utm_campaign=Badge_Grade)
![Beer](https://img.shields.io/beerpay/hashdog/scrapfy-chrome-extension.svg)

# Настольные часы на ИН-12
## Ключевые особенности
* Связка STM32F103 (Master) + ESP8266 (Slave) по SPI
* Датчик температуры (DS18B20)
* Датчик освещенности (BH1750, для динамической яркости)
* Web-панель, автоподстройка времени по WiFi
* Мультиплексирование ламп, индивидуальное управление яркостью ламп в широком диапазоне
* Индивидуальная RGB подсветка ламп (WS2812B)
## Железо
* STM32F103C8T6
* ESP8266 (ESP-07S)
* DS18B20, BH1750
* LMR23630, MAX1771
## IDE
* STM32: IAR EWARM (Kickstart)
* ESP8266: Eclipse CDT
## Внешний вид плат
![Основная плата сверху](https://github.com/jingobo/nixie-clock/blob/master/meta/Images/pcb_primary_top.jpg?raw=true)
![Основная плата снизу](https://github.com/jingobo/nixie-clock/blob/master/meta/Images/pcb_primary_bottom.jpg?raw=true)
![Вторичная плата в 3D](https://github.com/jingobo/nixie-clock/blob/master/meta/Images/pcb_secondary_3d.jpg?raw=true)
## Статус проекта
PCB не финальные, есть небольшой TODO список по доработкам, но схема и размеры меняться не будут).
- [x] Платы закончены
- [x] Базовые модули в STM32 и ESP8266
- [x] Связь между контроллерами по SPI
- [x] Базовые фильтры вывода ламп, неонок, подсветки
- [ ] SNTP на этапе отладки
- [ ] Web сервер с сокетами на этапе отладки
- [ ] Web панель на этапе верстки (Bootstrap)
