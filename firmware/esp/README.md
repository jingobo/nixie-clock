# Установка SDK
- Установить Node JS 20 и перезагрузить ПК
- Перейти в директорию ^\firmware\esp\meta\web в командной строке
- Выполнить команды по очереди: **md dist**, **npm install**, **npm run build-prod**
- Склонировать [ESP-IDF](https://github.com/jingobo/esp8266-rtos-sdk) в любую директорию
- Создать переменную среды **IDF_PATH** со значением пути к директории ESP-IDF
##### Распаковать из [архива](https://cloud.mail.ru/public/2oPD/8kE7GAKgK) в любую директорию
- Eclipse
- msys2
- xtensa-lx106-elf
##### Добавить в переменную среды PATH
- msys32\mingw32\bin
- msys32\usr\bin
- xtensa-lx106-elf\bin

##### Программирование прошивки
- Замкнуть RESET основного контроллера на замлю либо остановить под отладчиком
- В Eclipse открыть текущую дирнекторию и запустить по очереди цели **flash** и **romfs**