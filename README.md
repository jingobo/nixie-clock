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
## Статус проекта
С железом всё отлажено, завершено. PCB не финальные, есть небольшой TODO список по доработкам, но схема и размеры меняться не будут).
В прошивке STM32 на данный момент заложены базовые модули, отлажено управление лампами, софтовые таймеры, ватчдог и тактирование, полностью отлажена связь между чипами, реализовано хранилище настроек.
В прошивке ESP8266 на данный момент настроена сборка, добавлена синхронизация времени, уже запрашивает список хостов, отдает время.
