[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

# Настольные часы на ИН-12
## Ключевые особенности
* Связка STM32F103 (Master) + ESP8266 (Slave) по SPI
* ~~Датчик температуры (DS18B20)~~
* Датчик освещенности (BH1750, для динамической яркости)
* Web-панель, автоподстройка времени по WiFi
* Мультиплексирование ламп, индивидуальное управление яркостью ламп в широком диапазоне
* Индивидуальная RGB подсветка ламп (WS2812B)
## Hardware
* STM32F103C8T6
* ESP8266 (ESP-07S)
* DS18B20, BH1750
* LMR23630, MAX1771
## Web
* Node JS 20
* Webpack 5
## IDE
* STM32: IAR EWARM (Kickstart)
* ESP8266: Eclipse CDT
## Внешний вид плат
![Основная плата сверху](https://github.com/jingobo/nixie-clock/blob/master/meta/Images/pcb_primary_top.jpg?raw=true)
![Основная плата снизу](https://github.com/jingobo/nixie-clock/blob/master/meta/Images/pcb_primary_bottom.jpg?raw=true)
![Вторичная плата в 3D](https://github.com/jingobo/nixie-clock/blob/master/meta/Images/pcb_secondary_3d.jpg?raw=true)
![Корпус из оргстекла](https://github.com/jingobo/nixie-clock/blob/master/meta/Images/case_perspective.jpg?raw=true)
## Статус проекта
PCB не финальные, есть небольшой TODO список по доработкам, но схема и размеры меняться не будут).
Измерение температуры происходит не корректно из-за не удачного расположения датчика и конструктива корпуса.
- [x] Платы закончены
- [x] ~~Запущно измерение температуры~~
- [x] Запущно измерение освещенности
- [x] Базовые модули в STM32 и ESP8266
- [x] Связь между контроллерами по SPI
- [x] Базовые фильтры вывода ламп, неонок, подсветки
- [x] SNTP реализован, время синхронизируется
- [x] Web сервер со внутреннй файловой системой
- [x] Web сокеты реализованы, протокол проброшен
- [x] Web панель настроек времени
- [x] Web панель настроек WiFi
- [x] Web панель настроек дисплея
