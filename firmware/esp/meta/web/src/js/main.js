import "../index.html";
import "../css/main.css";

import "./color-picker";
import "./bootstrap.min";

import $ from "./jquery.min"; 
import Colors from "./colors";
import { NeonTube, NixieTube } from "./lamps";
import { LoadCounter, BinReader, BinWriter, utils, log } from "./utils";

const app = 
{
    // Признак отладки
    debug: window.location.hostname == 'localhost',
};

// Коды команд
app.opcode =
{
    // Код ошибки для переотправки
    IPC_RETRY: 0,
    // Запрос текущей даты/времени
    STM_TIME_GET: 2,
    // Установка текущей даты/времени
    STM_TIME_SET: 3,
    // Синхронизация даты/времени
    STM_TIME_SYNC_START: 4,
    
    // Запрос настроек даты/времени
    STM_TIME_SETTINGS_GET: 5,
    // Установка настроек даты/времени
    STM_TIME_SETTINGS_SET: 6,
    
    // Отчет о присвоении IP
    STM_WIFI_IP_REPORT: 7,
    // Запрос настроек WiFi
    STM_WIFI_SETTINGS_GET: 8,
    // Установка настроек WiFi
    STM_WIFI_SETTINGS_SET: 9,

    // Получает состояние экрана
    STM_SCREEN_STATE_GET: 10,

    // Запрос настроек сцены времени
    STM_DISPLAY_TIME_GET: 11,
    // Установка настроек сцены времени
    STM_DISPLAY_TIME_SET: 12,

    // Получает состояние освещенности
    STM_LIGHT_STATE_GET: 13,
    // Получает настройки освещенности
    STM_LIGHT_SETTINGS_GET: 14,
    // Задает настройки освещенности
    STM_LIGHT_SETTINGS_SET: 15,

    // Производит запуск прогрева ламп
    STM_HEAT_LAUNCH_NOW: 16,
    // Получает настройки прогрева ламп
    STM_HEAT_SETTINGS_GET: 17,
    // Задает настройки прогрева ламп
    STM_HEAT_SETTINGS_SET: 18,
    
    // Получает настройки сцены даты
    STM_DISPLAY_DATE_GET: 19,
    // Задает настройки сцены даты
    STM_DISPLAY_DATE_SET: 20,
    
    // Получает настройки сцены своей сети
    STM_ONET_SETTINGS_GET: 21,
    // Задает настройки сцены своей сети
    STM_ONET_SETTINGS_SET: 22,

    // Получает настройки сцены подключенной сети
    STM_CNET_SETTINGS_GET: 23,
    // Задает настройки сцены подключенной сети
    STM_CNET_SETTINGS_SET: 24,
    
    // Запрос информации о сети
    ESP_WIFI_INFO_GET: 26,    
    // Поиск сетей с опросом состояния
    ESP_WIFI_SEARCH_POOL: 27,    
    // Оповещение, что настройки WiFi сменились
    ESP_WIFI_SETTINGS_CHANGED: 28,
    
    // Запрос даты/времени из интернета
    ESP_TIME_SYNC: 29,
    // Передача списка хостов SNTP
    ESP_TIME_HOSTLIST_SET: 30,
};

// Оверлей
app.overlay = new function ()
{
    // Показывает/скрывает панель оверлея
    function toggle(show)
    {
        app.dom.container.visible(!show);
        app.dom.overlay.container.visible(show);
    };

    const errorClass = "error";
    
    // Показать/скрыть как обычная загрузка
    this.asLoader = show =>
    {
        app.dom.overlay.container.removeClass(errorClass);
        toggle(show);
    };
    
    // Показать как фатальную ошибку
    this.asError = msg =>
    {
        app.dom.overlay.container.addClass(errorClass);
        toggle(true);
        if (msg == undefined)
            return;
        app.dom.overlay.message.text(msg);
    };
};

// Всплывающее окно
app.popup = new function ()
{
    // Признак возможности показа
    let canShow = false;
    
    // Показ всплывающего окна
    this.show = msg =>
    {
        if (msg == undefined || !canShow)
            return;
        
        app.dom.popup.message.text(msg);
        app.dom.popup.container.modal();
    };

    // Событие загрузки приложения
    this.loaded = () => canShow = true;
    
    // Событие выгрузки приложения
    this.unloaded = () =>
    {
        canShow = false;
        app.dom.popup.container.modal('hide');
    };
};

// Список DOM элементов
app.dom = new function ()
{
    // Флаг ошибки
    let error = false;
    
    // Внутренняя функция ковертирования карты в DOM элементы
    function convert(dom, map)
    {
        for (let prop in map) 
        {
            const value = map[prop];
            
            if (typeof value == "object")
            {
                const obj = { };
                dom[prop] = obj
                convert(obj, value);
                continue;
            }
            
            if (typeof value == 'string')
            {
                const obj = $(value);
                if (obj.length <= 0)
                    error = true;
                dom[prop] = obj
                continue;
            }
            
            error = true;
        }
    };
    
    // Карта селекторов
    const map = 
    {
        display: "#display",
        container: ".container",
        
        popup:
        {
            container: "#popup",
            message: "#popup div.modal-body span",
        },

        overlay:
        {
            container: "#overlay",
            message: "#overlay .text-danger",
        },
        
        time:
        {
            hour: "#date-manual-tm > select:eq(0)",
            minute: "#date-manual-tm > select:eq(1)",
            second: "#date-manual-tm > select:eq(2)",

            day: "#date-manual-dt > select:eq(0)",
            month: "#date-manual-dt > select:eq(1)",
            year: "#date-manual-dt > select:eq(2)",
            
            ntp:
            {
                enable: "#date-ntp-enable",
                timezone: "#date-ntp-timezone",
                offset: "#date-ntp-offset",
                list: "#date-ntp-list",
                apply: "#date-ntp-apply",
                sync: "#date-ntp-sync",
                
                notice:
                {
                    container: "#date-ntp-notice",
                    message: "#date-ntp-notice-text",
                },
            },
            
            run:
            {
                message: "#date-run-notice-text",
            },
            lse:
            {
                message: "#date-lse-notice-text",
            },
        },
        
        wifi:
        {
            ap:
            {
                ssid: "#wifi-ap-ssid",
                pass: "#wifi-ap-pass",
                addr: "#wifi-ap-addr",
                group: "#wifi-ap-group",
                apply: "#wifi-ap-apply",
                enable: "#wifi-ap-enable",
                channel: "#wifi-ap-channel",
            },
            
            sta:
            {
                find: "#wifi-sta-find",
                list: "#wifi-sta-list",
            },
        },
        
        disp:
        {
            template:
            {
                main: "#disp-main-template",
                timeout: "#disp-timeout-template",
            },
            
            time:
            {
                holder: "#disp-time-holder",
            },
            
            light:
            {
                auto: "#disp-light-auto",
                night: "#disp-light-night",
                current: "#disp-light-current",
                manual:
                {
                    slider: "#disp-light-manual",
                    carrier: "#disp-light-manual-carrier",
                    label: "#disp-light-manual-carrier label",
                },
                smooth:
                {
                    slider: "#disp-light-smooth",
                    carrier: "#disp-light-smooth-carrier",
                    label: "#disp-light-smooth-carrier label",
                },
            },
            
            heat:
            {
                hour: "#disp-heat-hour",
                days:
                {
                    button: "#disp-heat-days button",
                    menu: "#disp-heat-days .dropdown-menu",
                    template: "#disp-heat-days template",
                },
                
                launch: "#disp-heat-launch",
            },
            
            date:
            {
                holder: "#disp-date-holder",
                enable: "#disp-date-enable",
            },

            onet:
            {
                holder: "#disp-onet-holder",
                enable: "#disp-onet-enable",
            },
            
            cnet:
            {
                holder: "#disp-cnet-holder",
                enable: "#disp-cnet-enable",
            },
        },
    };
    
    // Инициализация
    this.init = () =>
    {
        convert(this, map);
        if (error)
            throw "Ошибка инициализации макета";
    };
};

// Класс исходящего пакета
function Packet(opcode, name)
{
    // Проверка аргументов
    log.assert(opcode > 0 && opcode < 256, "Wrong opcode");
    
    // Инициализация полей
    this.opcode = opcode;
    this.data = new BinWriter();
    this.name = name;
    
    // Добавляем байт команды
    this.data.uint8(opcode);
}

// Объект дисплея неоновых ламп
app.display = new function ()
{
    // Полотно
    let canvas;
    // Конекст вывода
    let context;
    // Интервал вывода
    let redrawInterval;
    // Таймаут запроса
    let requestTimeout;

    // Количество неонок
    const neonCount = 4;
    // Количество ламп
    const nixieCount = 6;
    
    // Список плавных переходов
    const smooths = [];

    // Класс плавного перехода
    function Smooth(getter, setter)
    {
        smooths.push(this);
        
        // Локальные данные
        let frame 
        let toValue;
        let fromValue;
        const frameCount = 5;
        
        // Обработка эффекта
        this.process = () =>
        {
            setter(fromValue + (toValue - fromValue) * (frame / frameCount));
            if (frame < frameCount)
                frame++;
        };
        
        // Запуск эффекта
        this.start = value =>
        {
            if (toValue == value)
                return;
            
            frame = 0;
            toValue = value;
            fromValue = getter();
        };
        
        // Начальная инициализация
        this.start(getter());
    }

    // Инициализация ламп
    const nixies = utils.range(nixieCount).map(() =>
        {
            const n = new NixieTube();
            
            // Инициализация эффектов
            n.satSmooth = new Smooth(() => n.sat, v => n.sat = v);
            n.led.rSmooth = new Smooth(() => n.led.r, v => n.led.r = v);
            n.led.gSmooth = new Smooth(() => n.led.g, v => n.led.g = v);
            n.led.bSmooth = new Smooth(() => n.led.b, v => n.led.b = v);
            n.led.aSmooth = new Smooth(() => n.led.a, v => n.led.a = v);
            
            return n;
        });

    // Инициализация неонок
    const neons = utils.range(neonCount).map(() =>
        {
            const n = new NeonTube();
            
            // Инициализация эффектов
            n.satSmooth = new Smooth(() => n.sat, v => n.sat = v);
            
            return n;
        });
    
    // Обновление дисплея
    function redraw()
    {
        // Обработка эффектов плавного перехода
        smooths.forEach(s => s.process());
        
        // Отчистка экрана
        context.fillRect(0, 0, canvas.width, canvas.height);
        // Вывод ламп
        context.save();
            context.translate(20, 5);
            let ni = 0;
            function drawNeon()
            {
                if (ni >= neonCount)
                    return;
                
                neons[ni++].draw(context);
            };
            
            for (let i = 0; i < nixies.length; i++)
            {
                // Основная лампа
                nixies[i].draw(context);
                context.translate(154, 0);
                
                if (i % 2 == 0)
                {
                    context.translate(6, 0);
                    continue;
                }
                
                // Неонки
                context.save();
                    context.translate(0, 77);
                    drawNeon();
                    
                    context.translate(0, 68);
                    drawNeon();
                context.restore();
                
                context.translate(36, 0);
            }
        context.restore();
    };
    
    // Признак обновления данных после запуска
    let updated = false;

    // Запрос состояния
    async function request()
    {
        const data = await app.session.transmit(new Packet(app.opcode.STM_SCREEN_STATE_GET, "получение состояния экрана"));
        if (data == null)
            return;
        updated = true;
        
        // Данные ламп
        nixies.forEach(tube =>
            {
                tube.digit = data.uint8();
                tube.satSmooth.start(data.uint8());
                tube.dot = data.bool();
            });
        
        // Данные неонок
        neons.forEach(neon => 
            neon.satSmooth.start(data.uint8() > 127 ? 
                255 : 
                0));

        // Данные подсветки
        nixies.forEach(tube =>
            {
                const g = data.uint8();
                const r = data.uint8();
                const b = data.uint8();
                
                tube.led.rSmooth.start(r);
                tube.led.gSmooth.start(g);
                tube.led.bSmooth.start(b);
                
                tube.led.aSmooth.start(Math.max(r, g, b) * 0.65);
            });
                
        // Перезапрос
        requestTimeout = setTimeout(request, 100);
    };
    
    // Инициализация
    this.init = () =>
    {
        const error = "Нет поддержки HTML5 Canvas";
        // Получаем элемент Canvas
        canvas = app.dom.display[0];
        // Получаем метод
        if (canvas.getContext == undefined)
            throw error;
        // Получаем контекст
        context = canvas.getContext("2d", { alpha: false });
        if (context == null)
            throw error;
        // Начальная настройка канвы
        context.fillStyle = "white";
    };
    
    // Загрузка страницы
    this.loaded = () =>
    {
        // Снимаем признак обновления данных
        updated = false;
        // Старт цикла обновления дисплея
        redrawInterval = setInterval(redraw, 1000 / 50);
        // Начальный запрос
        request();
    };
    
    // Выгрузка страницы
    this.unloaded = () =>
    {
        clearTimeout(requestTimeout);
        clearInterval(redrawInterval);
    };

    // Получает признак готовности дисплея
    this.ready = () => updated;
};

// Страницы
app.page = 
{
    // Страница даты/времени
    time: new function ()
    {
        // Счетчик загруженных пакетов
        const loadCounter = new LoadCounter(2);
        // Время работы устройства
        let uptime_seconds = 0;
        // Время синхронизации с часами
        let syncTime = new Date();
        // Время прочтенное с часов
        let readedTime = new Date();
        // Время с завершения изменения даты/времени
        let modifyTime = new Date(0);
        
        // Запрос текущей даты/времени
        async function requestTime()
        {
            const data = await app.session.transmit(new Packet(app.opcode.STM_TIME_GET, "запрос даты/времени"), loadCounter);
            if (data == null)
                return;

            // Декодирование
            syncTime = new Date();
            
            // Текущая дата
            {
                const year = data.uint8();
                const month = data.uint8();
                const day = data.uint8();
                const hour = data.uint8();
                const minute = data.uint8();
                const second = data.uint8();
                readedTime = new Date(year + 2000, month - 1, day, hour, minute, second);
            }
            
            // Дата синхронизации
            {
                const year = data.uint8();
                const month = data.uint8();
                const day = data.uint8();
                const hour = data.uint8();
                const minute = data.uint8();
                const second = data.uint8();

                // Вывод даты
                const notice = app.dom.time.ntp.notice;
                if (month != 0)
                {
                    notice.message.text(
                        utils.leadingZeros(day, 2) + "." +
                        utils.leadingZeros(month, 2) + "." +
                        utils.leadingZeros(year, 2) + " в " +
                        utils.leadingZeros(hour, 2) + ":" +
                        utils.leadingZeros(minute, 2) + ":" +
                        utils.leadingZeros(second, 2));
                    notice.container.visible(true);
                }
                else
                    notice.container.visible(false);
            }
            
            // Время работы устройства
            uptime_seconds = data.uint32();
            
            // Частота LSE
            app.dom.time.lse.message.text(data.uint16());
            
            // Возможность синхронизации
            app.dom.time.ntp.sync.disabled(!data.bool());
            
            log.info("Received current datetime: " + readedTime + "...");
            updateTimeOnDom();
        };

        // Запрос настроек
        async function requestSettings()
        {
            app.dom.time.ntp.apply.spinner(false);
            
            // Запрос
            const data = await app.session.transmit(new Packet(app.opcode.STM_TIME_SETTINGS_GET, "запрос настроек синхронизации"), loadCounter);
            if (data == null)
                return;
            
            // Декодирование
            app.dom.time.ntp.enable.checked(data.bool());
            app.dom.time.ntp.timezone.val(data.int8());
            app.dom.time.ntp.offset.val(data.int8());
            app.dom.time.ntp.list.val(data.cstr(260));
            log.info("Received datetime sync settings...");
        };
        
        // Обновление текущего времени на дисплее
        function updateTimeOnDom()
        {
            // Время работы устройства
            {
                let uptime = uptime_seconds;
                
                // Функция деления частей времени
                const div = (v) =>
                {
                    const result = uptime % v;
                    uptime = Math.floor(uptime / v);
                    return result;
                };
                
                const seconds = div(60);
                const minutes = div(60);
                const hours = div(24);
                const days = uptime;
                
                // Вывод времени
                app.dom.time.run.message.text(
                    days + " дней " +
                    utils.leadingZeros(hours, 2) + ":" +
                    utils.leadingZeros(minutes, 2) + ":" +
                    utils.leadingZeros(seconds, 2));
            }
            
            // Текущее время
            const time = app.page.time.currentTime();
            const yy = time.getFullYear();
            const nn = time.getMonth();
            const dd = time.getDate();
            const hh = time.getHours();
            const mm = time.getMinutes();
            const ss = time.getSeconds();
            
            // Можем ли модифицировать поля ввода
            const dx = new Date() - modifyTime;
            if (dx < 5000)
                return;
            
            app.dom.time.second.val(ss);
            app.dom.time.minute.val(mm);
            app.dom.time.hour.val(hh);
            app.dom.time.day.val(dd);
            app.dom.time.month.val(nn);
            app.dom.time.year.val(yy);
        };

        // Класс автомата синхронизации
        const synchronizer = new function ()
        {
            // Флаг состояния работы
            let running = false;
            
            // Передача запроса
            const transmit = async check =>
            {
                log.assert(running);
                
                // Подготовка пакета
                const packet = new Packet(app.opcode.STM_TIME_SYNC_START, "синхронизация даты/времени");
                packet.data.bool(check);
                
                // Запрос
                const data = await app.session.transmit(packet);
                if (data == null || !running)
                {
                    this.reset();
                    return;
                }
                
                // Декодирование
                switch (data.uint8())
                {
                    // Успех
                    case 0:
                        requestTime();
                        log.info("Date/time syncronization success.");
                        break;
                        
                    // Ошибка
                    case 1:
                        app.popup.show("Не возможно выполнить синхронизацию!");
                        log.info("Date/time syncronization fail!");
                        break;
                        
                    // Обработка
                    case 2:
                        if (running)
                            transmit(true);
                        log.info("Date/time syncronization pending...");
                        return;
                }
                
                this.reset();
            };
            
            // Запуск автомата
            this.run = () =>
            {
                if (running)
                    return;
                running = true;
                
                transmit();
                app.dom.time.ntp.sync.spinner(true);
                log.info("Start date/time syncronization...");
            };
            
            // Сброс автомата
            this.reset = () =>
            {
                running = false;
                app.dom.time.ntp.sync.spinner(false);
            };
        };

        // Инициализация страницы
        this.init = () =>
        {
            // Заполнение полей даты/времени
            utils.dropdownFillNumber(app.dom.time.hour, 0, 23, 2);
            utils.dropdownFillNumber(app.dom.time.minute, 0, 59, 2);
            utils.dropdownFillNumber(app.dom.time.second, 0, 59, 2);
            utils.dropdownFillNumber(app.dom.time.day, 1, 31);
            utils.dropdownFillArray(app.dom.time.month, 
                [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11], 
                ["январь", "февраль", "март", "апрель", "май", "июнь", "июль", "август", "сентябрь", "октябрь", "ноябрь", "декабрь"]);
            utils.dropdownFillNumber(app.dom.time.year, 2000, 2099);
            
            // Обработчик событий полей текущей даты/времени
            app.dom.time.hour
                .add(app.dom.time.minute)
                .add(app.dom.time.second)
                .add(app.dom.time.day)
                .add(app.dom.time.month)
                .add(app.dom.time.year).change(async () =>
                {
                    modifyTime = new Date();
                    
                    // Отправляем пакет с установкой даты/времени
                    const packet = new Packet(app.opcode.STM_TIME_SET, "применение даты/времени");
                    packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.year) - 2000);
                    packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.month) + 1);
                    packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.day));
                    packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.hour));
                    packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.minute));
                    packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.second));
                    
                    // Запрос
                    const data = await app.session.transmit(packet);
                    if (data == null)
                        modifyTime = new Date(0);
                    
                    // Перезапрашивает текущую дату/время
                    requestTime();
                });
            
            // Обновление времени на дисплее
            setInterval(updateTimeOnDom, 500);
            
            // Обновление времени работы устройства
            setInterval(() => uptime_seconds++, 1000);

            // Обработчик клика по кнопке применения настроек
            app.dom.time.ntp.apply.click(async () =>
                {
                    app.dom.time.ntp.apply.spinner(true);
                    
                    // Отправляем пакет с установкой даты/времени
                    const packet = new Packet(app.opcode.STM_TIME_SETTINGS_SET, "применение настроек синхронизации");
                    packet.data.bool(app.dom.time.ntp.enable.checked());
                    packet.data.int8(app.dom.time.ntp.timezone.val());
                    packet.data.int8(app.dom.time.ntp.offset.val());
                    packet.data.cstr(utils.lf(app.dom.time.ntp.list.val().trim()), 260);
                    
                    // Запрос
                    await app.session.transmit(packet);
                    
                    // Перезапрашивает текущие настройки и дату/время
                    requestTime();
                    requestSettings();
                });
            
            // Обработчик клика по кнопке синхронизации даты/времени
            app.dom.time.ntp.sync.click(synchronizer.run);
        };
        
        // Загрузка страницы
        this.loaded = () =>
        {
            loadCounter.reset();
            // Запрос даты/времени и настроек
            requestTime();
            requestSettings();
            // Сброс автомата синхронизации
            synchronizer.reset();
        };
        
        // Получает, готова ли страница к отображению
        this.ready = () => loadCounter.ready;
        
        // Получает текущую дату/время
        this.currentTime = () =>
        {
            let dx = new Date();
            dx -= syncTime;
            return new Date(readedTime.getTime() + dx);
        };
    },
    
    // Страница настроек WiFi
    wifi: new function ()
    {
        // Блокировка/Разблокировка всех элементов подключения
        function setConnectLock(state)
        {
            app.dom.wifi.ap.apply.disabled(state);
            app.dom.wifi.sta.find.disabled(state);
            $("#wifi-sta-list .card-body input").disabled(state);
            $("#wifi-sta-list .card-body button").disabled(state);
        }

        // Устанавливает адрес ссылке
        function setLinkAddress(link, address)
        {
            link.text(address);
            link.prop("href", "http://" + address);
        }
        
        // Валидация значения пароля
        function validateStationPassword(input, allowEmpty)
        {
            const value = input.val();
            
            let result = true;
            if (allowEmpty)
            {
                if ((value.length > 0) && (value.length < 8 || value.length > 12))
                    app.popup.show("Пароль должен либо отсутствовать либо содержать от 8 до 12 символов!");
                else
                    result = false;
            }
            else if (value.length <= 0)
                app.popup.show("Пароль не введен!");
            else if (value.length < 8 || value.length > 12)
                app.popup.show("Пароль должен содержать от 8 до 12 символов!");
            else
                result = false;
            
            input.setClass("is-invalid", result);
            return result;
        }
        
        // Класс хранения настроек
        function Settings()
        {
            // Использование
            this.use = false;
            // Канал
            this.channel = 0;
            // Имя
            this.ssid = "";
            // Пароль
            this.password = "";
            
            // Загрузка из бинарных данных
            this.load = data =>
            {
                this.use = data.bool();
                this.channel = data.uint8();
                this.ssid = data.cstr(33);
                this.password = data.cstr(13);
            };
            
            // Сохранение в бинарные данные
            this.save = data =>
            {
                data.bool(this.use);
                data.uint8(this.channel);
                data.cstr(this.ssid, 33);
                data.cstr(this.password, 13);
            };
        }

        // Настройки точки доступа
        const apSettings = new Settings();
        // Настройки подключения к сети
        const staSettings = new Settings();
        
        // Состояние подключения
        const STATION_STATE_CONNECT = 0;
        // Состояние отключения
        const STATION_STATE_DISCONNECT = 2;
        // Состояние забыть
        const STATION_STATE_FORGET = 3;
        
        // Признак загрузки настроек
        let settingsUploaded = false;
        
        // Класс информации о точке доступа
        function Station(parent)
        {
            // Плавный показ
            parent.slideDown("fast");
            // Адрес интерфейса
            const link = parent.find("a");
            // Заголовок
            const header = parent.find(".card-header");
            // Кнопка аккордиона
            const headerButton = header.find("button");
            // Свойства сети
            const props = header.find("net-props");
            // Поле пароля
            const passInput = parent.find("input");
            // Предупреждение о ошибке подключения
            const connectAlert = parent.find(".alert");
            // Части ввода пароля
            const passParts = parent.find(".wifi-sta-psw-part");
            // Кнопка подключения
            const connectButton = parent.find(".btn-group > .btn");
            
            // Обновление видимости полей пароля
            const updatePassParts = () =>
                passParts.visible(this.priv && this.state == STATION_STATE_CONNECT);
            
            // Обработчик клика по кнопке подключения
            connectButton.click(async () =>
                {
                    // Заполнение настроек
                    const ns = new Settings();
                    ns.ssid = this.ssid;
                    ns.password = this.password;
                    
                    switch (this.state)
                    {
                        case STATION_STATE_CONNECT:
                            // Коррекция/Сброс пароля
                            if (this.priv)
                            {
                                if (validateStationPassword(passInput, false))
                                    return;
                            }
                            else
                                ns.password = "";
                            ns.use = true;
                            break;
                            
                        case STATION_STATE_FORGET:
                        case STATION_STATE_DISCONNECT:
                            break;
                            
                        default:
                            log.assert(false);
                            break;
                    }
                    
                    // Заполнение пакета
                    const packet = new Packet(app.opcode.STM_WIFI_SETTINGS_SET, "применение настроек WiFi");
                    ns.save(packet.data);
                    apSettings.save(packet.data);
                    
                    // Запрос
                    this.loading = true;
                    settingsUploaded = false;
                    await app.session.transmit(packet);
                    
                    // Перезапрос настроек
                    requestSettings();
                });
            
            // Имя сети
            let ssid;
            Object.defineProperty(this, "ssid", 
                {
                    get: () => ssid,
                    set: value =>
                    {
                        ssid = value;
                        header.find("span").text(ssid);
                    },
                    configurable: false
                });
            
            // Уровень сигнала
            let rssi = 0;
            Object.defineProperty(this, "rssi", 
                {
                    get: () => rssi,
                    set: value =>
                    {
                        rssi = value;
                        if (rssi >= 0)
                        {
                            props.visible(false);
                            return;
                        }
                        
                        props.visible(true);
                        header.find("small").text(rssi + " дБм");
                    },
                    configurable: false
                });
                
            // Признак приватности
            let priv = false;
            Object.defineProperty(this, "priv", 
                {
                    get: () => priv,
                    set: value =>
                    {
                        priv = value;
                        header.find("svg").visible(priv);
                        updatePassParts();
                    },
                    configurable: false
                });
                
            // Признак активности
            Object.defineProperty(this, "active", 
                {
                    get: () => headerButton.hasClass("active"),
                    set: value => headerButton.setClass("active", value),
                    configurable: false
                });

            // Пароль
            Object.defineProperty(this, "password", 
                {
                    get: () => passInput.val(),
                    set: value => passInput.val(value),
                    configurable: false
                });
                
            // Состояние
            let state = STATION_STATE_CONNECT;
            Object.defineProperty(this, "state", 
                {
                    get: () => state,
                    set: value =>
                    {
                        state = value;
                        // Сброс класса
                        connectButton.setClass("btn-success", false);
                        connectButton.setClass("btn-warning", false);
                        connectButton.setClass("btn-secondary", false);
                        
                        // Сброс предупреждения
                        connectAlert.visible(false);
                        
                        link.visible(false);
                        const span = connectButton.find("span");
                        switch (value)
                        {
                            case STATION_STATE_CONNECT:
                                span.text("Подключиться");
                                connectButton.setClass("btn-success", true);
                                break;
                                
                            case STATION_STATE_DISCONNECT:
                                link.visible(true);
                                span.text("Отключиться");
                                connectButton.setClass("btn-warning", true);
                                break;
                                
                            case STATION_STATE_FORGET:
                                connectAlert.visible(true);
                                span.text("Ввести другой пароль");
                                connectButton.setClass("btn-secondary", true);
                                break;
                                
                            default:
                                log.assert(false);
                                break;
                        }
                        
                        updatePassParts();
                    },
                    configurable: false
                });
                
            // Признак загрузки
            let loading = false;
            Object.defineProperty(this, "loading", 
                {
                    get: () => loading,
                    set: value => 
                    {
                        if (loading == value)
                            return;
                        
                        loading = value;
                        setConnectLock(value);
                        connectButton.spinner(value);
                    },
                    configurable: false
                });

            // Адрес
            Object.defineProperty(this, "address", 
                {
                    get: () => link.text(),
                    set: value => setLinkAddress(link, value),
                    configurable: false
                });
            
            // Признак удаления
            this.removing = false;
        }

        // Элемент списка чужих сетей
        let stations;
        
        // Добавление точки доступа
        function appendStation(ssid, priv, rssi)
        {
            let item;
            // Поиск сети в списке
            let index = stations.items.findIndex(i => i.station.ssid == ssid);
            if (index < 0)
            {
                // Не нашли, поиск места по уровню сигнала
                index = 0;
                if (rssi != undefined)
                    for (; index < stations.items.length; index++)
                        if (stations.items[index].station.rssi < rssi)
                            break;
                
                // Создание и вставка
                item = stations.create();
                item.list.insert(index);
                item.station = new Station(item);
            }
            else
                item = stations.items[index];
            
            // Для сокращения
            const station = item.station;
            // Признак удаления
            station.removing = false;
            
            // Данные сети
            station.ssid = ssid;
            station.priv = priv;
            if (rssi != undefined)
                station.rssi = rssi;
            else
            {
                station.active = true;
                station.password = staSettings.password;
            }
        }
        
        // Подчистка списка сетей (завершение)
        function cleanupStations()
        {
            // Снятие признака активности
            stations.items.forEach(i => i.station.active = false);
            // Добавление подключенной сети
            if (staSettings.use)
                appendStation(staSettings.ssid, staSettings.password.length > 0);
            
            // Запуск удаления
            stations.items
                // Удаляется сеть если есть признак удаления или если сигнал отсутствует и она при этом не основная
                .filter(i => i.station.removing || (!i.station.active && i.station.rssi >= 0))
                .forEach(i => i.slideUp("fast", () => i.list.remove()));
        }

        // Счетчик загруженных пакетов
        const loadCounter = new LoadCounter(2);
        
        // Поисковик сетей
        const searcher = new function ()
        {
            // Индекс сети
            let netIndex;
            // Признак запуска
            let running = false;
            
            // Сброс данных
            function reset()
            {
                running = false;
                app.dom.wifi.sta.find.spinner(false);
            }
            
            // Обработчик завершения поиска
            function finalize()
            {
                log.assert(running);
                
                loadCounter.increment();
                reset();
                cleanupStations();
                log.info("WiFi finder stopped...");
            };
            
            // Передача команды запуска
            const sendStart = () => sendCommand(0x01, "запуск поиска WiFi сетей");
            
            // Передача команды опроса
            const sendPool = () => sendCommand(0x00, "опрос поиска WiFi сетей");
            
            // Передача комадны
            async function sendCommand(command, message)
            {
                log.assert(running);
                
                // Подготовка пакета
                const packet = new Packet(app.opcode.ESP_WIFI_SEARCH_POOL, message);
                packet.data.uint8(command);
                packet.data.uint8(netIndex);
                
                // Запрос
                const data = await app.session.transmit(packet);
                
                // Игнор, если не запущенны
                if (!running)
                    return;
                
                // Если произошла ошибка
                if (data == null)
                {
                    finalize();
                    return;
                }
                
                // Разбор данных
                switch (data.uint8())
                {
                    // Простой
                    case 0x00:
                        // Если это бы запуск
                        if (command == 0x01)
                        {
                            // Повтор
                            sendStart();
                            return;
                        }
                        
                        // Завершение
                        finalize();
                        return;
                        
                    // Поиск
                    case 0x01:
                        // Опрос
                        sendPool();
                        return;
                        
                    // Запись
                    case 0x02:
                        {
                            // Чтение записи
                            const ssid = data.cstr(33);
                            const rssi = data.int8();
                            const priv = data.bool();
                            // Лог
                            log.info("Found WiFi network \"" + ssid + "\", rssi " + rssi + ", " + (priv ? "private" : "open"));
                            
                            appendStation(ssid, priv, rssi);
                        }
                        
                        // Опрос
                        netIndex++;
                        sendPool();
                        return;
                }
            };
            
            // Сброс
            this.reset = () =>
            {
                reset();
                stations.clear();
            };
            
            // Запуск поиска
            this.start = () =>
            {
                if (running)
                {
                    log.error("WiFi finder already running!");
                    return;
                }
                running = true;
                
                // Помечаем элементы к удалению
                stations.items.forEach(i => i.station.removing = true);
                
                app.dom.wifi.sta.find.spinner(true);
                log.info("WiFi finder running...");
                netIndex = 0;
                sendStart();
            };
            
        };
        
        // Запрос настроек
        async function requestSettings()
        {
            // Запрос
            const data = await app.session.transmit(new Packet(app.opcode.STM_WIFI_SETTINGS_GET, "запрос настроек WiFi"), loadCounter);
            if (data == null)
                return;
            
            settingsUploaded = true;
            log.info("Received WiFi settings...");
            
            // Декодирование
            staSettings.load(data);
            apSettings.load(data);
            
            // Новая сеть
            app.dom.wifi.ap.ssid.val(apSettings.ssid);
            app.dom.wifi.ap.pass.val(apSettings.password);
            app.dom.wifi.ap.enable.checked(apSettings.use);
            app.dom.wifi.ap.channel.val(apSettings.channel);
            
            // Подключенная сеть
            cleanupStations();
            // Обновление состояния
            requestState();
        }
        
        // Таймаут запроса состояния сети
        let requestStateTimeout;
        
        // Запрос состояния сети
        async function requestState()
        {
            clearTimeout(requestStateTimeout);
            
            // Запрос
            const data = await app.session.transmit(new Packet(app.opcode.ESP_WIFI_INFO_GET, "запрос состояния WiFi"), loadCounter);
            if (data == null)
                return;

            // Следующий перезапрос
            requestStateTimeout = setTimeout(requestState, 1000);
            
            // Выходим если настройки загружаются
            if (!settingsUploaded)
                return;
            
            // Чтение адреса
            function ip()
            {
                const o1 = data.uint8();
                const o2 = data.uint8();
                const o3 = data.uint8();
                const o4 = data.uint8();
                
                return o1 + "." + o2 + "." + o3 + "." + o4;
            }
            
            // Подключение к сети
                // Статус
            const stationState = data.uint8();
            const stationIp = ip();
            // Создание сети
                // Статус (пропуск)
            const softapState = data.uint8();
                // Адрес
            const softapIp = ip();
            {
                const CLASS_USE = "col-md-8";
                const CLASS_UNUSE = "col-md-10";
                
                // Признак активности
                const active = softapState == 2;
                
                // Сброс класса группы
                app.dom.wifi.ap.group.setClass(CLASS_USE, false);
                app.dom.wifi.ap.group.setClass(CLASS_UNUSE, false);
                
                // Установка класса группы
                app.dom.wifi.ap.group.setClass(active ?
                    CLASS_USE :
                    CLASS_UNUSE, true);
                    
                // Установка видимости адреса
                app.dom.wifi.ap.addr.visible(active);
            }
            
            setLinkAddress(app.dom.wifi.ap.addr, softapIp);
            
            // Обход сетей
            stations.items.forEach(i =>
            {
                const station = i.station;
                
                // Если не активная точка
                if (!station.active)
                {
                    station.loading = false;
                    station.state = STATION_STATE_CONNECT;
                    return;
                }
                
                // Если транзакция
                if (stationState == 1)
                    // Не обрабатываем
                    return;
                    
                // Если состояние не изменилось
                if (station.state == stationState)
                    return;
                
                station.state = stationState;
                station.loading = false;
                station.address = stationIp;
            });
        }
        
        // Инициализация страницы
        this.init = () =>
        {
            stations = app.dom.wifi.sta.list.makeTemplateList("wifi-sta-info", "wifi-sta-pass");
            
            // Обработчик клика по кнопке обновления списка сетей
            app.dom.wifi.sta.find.click(searcher.start);
            
            // Обработчик клика примнения настроек новое сети
            app.dom.wifi.ap.apply.click(async () =>
            {
                // Валидация имени
                {
                    const input = app.dom.wifi.ap.ssid;
                    const result = input.val().length <= 0;
                    
                    input.setClass("is-invalid", result);
                    if (result)
                    {
                        app.popup.show("Имя сети должно содержать минимум 1 символ!");
                        return;
                    }
                }
                
                // Валидация пароля
                if (validateStationPassword(app.dom.wifi.ap.pass, true))
                    return;
                
                // Заполнение настроек
                const ns = new Settings();
                ns.ssid = app.dom.wifi.ap.ssid.val();
                ns.password = app.dom.wifi.ap.pass.val();
                ns.use = app.dom.wifi.ap.enable.checked();
                ns.channel = app.dom.wifi.ap.channel.val();
                
                // Заполнение пакета
                const packet = new Packet(app.opcode.STM_WIFI_SETTINGS_SET, "применение настроек WiFi");
                staSettings.save(packet.data);
                ns.save(packet.data);
                
                // Запрос
                setConnectLock(true);
                app.dom.wifi.ap.apply.spinner(true);
                    await app.session.transmit(packet);
                setConnectLock(false);
                app.dom.wifi.ap.apply.spinner(false);
                
                // Перезапрос настроек
                requestSettings();
            });
            
            // Включение/Отключение полей новой сети
            {
                const ap = app.dom.wifi.ap;
                
                // Обработчик изменения состояния активности новой сети
                function changeHandler()
                {
                    const state = !ap.enable.checked();
                    
                    ap.ssid.disabled(state);
                    ap.pass.disabled(state);
                    ap.channel.disabled(state);
                }
                
                ap.enable.change(changeHandler);
                changeHandler();
            }
            
        };
        
        // Загрузка страницы
        this.loaded = () =>
        {
            app.dom.wifi.ap.apply.spinner(false);
            setConnectLock(false);
            loadCounter.reset();
            requestSettings();
            requestState();
            searcher.reset();
            searcher.start();
        };
        
        // Выгрузка страницы
        this.unloaded = () =>
        {
            clearTimeout(requestStateTimeout);
        };
        
        // Получает, готова ли страница к отображению
        this.ready = () => loadCounter.ready;
    },
    
    // Страница настроек дисплея
    display: new function ()
    {
        // Счетчик загруженных пакетов
        const loadCounter = new LoadCounter(6);
        
        // Класс основных настроек дисплея
        function DisplaySettings()
        {
            // Инициализация DOM
            this.dom = app.page.display.mainTemplate();
            
            // Обработчик события изменения
            this.onchange = () => { };
            
            // Обработчик вызова события изменения
            const fire = () => this.onchange();

            // Инициализация слейдеров продолжительности
            this.dom.find(".labeled-slider").setupDurationSlider();
            
            // Режим эффекта цифр
            const digitEffect = this.dom.find(".disp-class-digit-effect");
            digitEffect.change(fire);
            
            // Начальный цвет
            const ledInitialColor = new Colors({ color: "#FFFFFF"});
            // Инициализация разрядов цветов подсветки
            const ledRanks = this.dom
                .find(".color-picker")
                .toArray()
                .map(element =>
                    {
                        // Для определения реального изменения цвета
                        let lastColor;

                         // Подготовка объекта разряда
                        const result = { };
                       
                        // Биндинг пипетки
                        const picker = $(element).colorPicker(
                            {
                                GPU: true,
                                opacity: false,
                                cssPrepend: true,
                                animationSpeed: 150,
                                renderCallback()
                                {
                                    // Определение реального изменения цвета
                                    if (lastColor.colors.HEX == result.color.colors.HEX)
                                        return;
                                    lastColor = result.color;
                                    
                                    // Установка цвета всем разрядам если выбран режим
                                    if (ledOneColor.checked())
                                        ledRanks.forEach(r => r.color = result.color);
                                    
                                    // Генерация события
                                    fire();
                                }
                            });
                        
                        // Свойство цвета
                        Object.defineProperty(result, "color", 
                            {
                                get: () => new Colors({ color: picker.css("background-color") }),
                                set: value => 
                                    {
                                        lastColor = new Colors({ color: value.colors.HEX});
                                        picker.css({"background-color": element.value = value.toString("rgb", false) });
                                    },
                                configurable: false
                            });
                            
                        result.color = ledInitialColor;
                        
                        // Свойство видимости
                        Object.defineProperty(result, "visible", 
                            {
                                get: () => picker.visible(),
                                set: value => picker.visible(value),
                                configurable: false
                            });
                        
                        return result;
                    });
                
            // Надпись разрядов подсветки
            const ledPickerLabel = this.dom.find(".color-picker-label");
            
            // Оверлей разрядов подсветки
            const ledPickerOverlay = this.dom.find(".color-picker-overlay");            
            
            // Кнопка случайных цветов
            const ledRandomColors = this.dom.find(".disp-class-led-rnd-color");
            ledRandomColors.click(() =>
                {
                    // Для сокращения
                    const r = Math.random;
                    // Буфер цвета
                    const colorBuffer = new Colors();
                    // Рандом [75...100]
                    const rr = () => 75 + r() * 15;
                    
                    // Обработчик совсем случайных цветов
                    function rand()
                    {
                        ledRanks.forEach(rank => 
                            {
                                // Установка цвета
                                colorBuffer.setColor({ h: r() * 360, s: rr(), v: 100 }, "hsv");
                                rank.color = colorBuffer;
                            });
                    }
                    
                    // Обработчик комплиментарных случайных цветов
                    function comp()
                    {
                        // Начальные составляющие
                        let h = r() * 60;
                        const s = rr();
                        const v = 100;
                        
                        // Перемешка индексов разрядов
                        utils.range(ledRanks.length)
                            .shuffle()
                            .forEach(i =>
                                {
                                    // Установка цвета
                                    colorBuffer.setColor({ h: h, s: s, v: v }, "hsv");
                                    ledRanks[i].color = colorBuffer;
                                    
                                    // Следующий комплиментарный цвет
                                    h += 60;
                                });
                    }
                    
                    // Случайно выбираем обработчик рандомизации
                    if (r() > 0.5)
                        comp();
                    else
                        rand();
                    
                    // Генерация события
                    fire();
                });
                
            // Обновление включенности кнопки рандомизации цвета
            const updateLedRandomColorsEnabled = () =>
                ledRandomColors.disabled(ledOneColor.checked() || ledModeUsingRandom());
            
            // Чекбокс одного цвета
            const ledOneColor = this.dom.find(".disp-class-led-one-color");
            const ledOneColorChanged = () =>
            {
                // Новое состояние
                const state = ledOneColor.checked();
                // Применение режима
                const visible = !state;
                ledEffect.disabled(state);
                
                // Расчет среднего цвета
                const avgColor = ledRanks[(Math.random() * 5).toFixed()].color;
                
                // Обход разрядов
                for (let i = 0; i < ledRanks.length; i++)
                {
                    const rank = ledRanks[i];
                    
                    // Применение среднего цвета
                    if (state)
                        rank.color = avgColor;

                    // Применение видимости не первого разряда
                    if (i > 0)
                        rank.visible = visible;
                }
                
                // После установки общего цвета
                if (state)
                    ledEffect.val(0).change();
                updateLedRandomColorsEnabled();
            };
            ledOneColor.change((info, args) =>
            {
                ledOneColorChanged();
                // Фильтр на проверку реального изменения пользователем
                if (args !== undefined || ledOneColor.checked())
                    return;
                
                // Ранзомизация цветов
                ledRandomColors.click();
            });

            // Надпись режима подсветки
            const ledModeLabel = this.dom.find(".disp-class-led-mode-label");
                
            // Режим подсветки
            const ledMode = this.dom.find(".disp-class-led-mode");
            const ledModeChanged = () =>
            {
                // Выбор цветов разрешен если выбраны указанные цвета
                const state = ledModeUsingRandom();
                // Установка состояния
                ledOneColor.disabled(state);
                updateLedRandomColorsEnabled();
                ledPickerLabel.setClass("disabled-label", state);
                ledPickerOverlay.visible(state);
            };
            ledMode.change(() =>
                {
                    ledModeChanged();
                    fire();
                });
            
            // Получает признак режима совсем случайных цветов
            const ledModeUsingRandom = () => ledMode.val() > 1;

            // Надпись плавности подсветки
            const ledSmoothLabel = this.dom.find(".disp-class-led-smooth-label");
                
            // Плавность подсветки
            const ledSmooth = this.dom.find(".disp-class-led-smooth");
            ledSmooth.on("input", fire);
                
            // Эффект подсветки
            const ledEffect = this.dom.find(".disp-class-led-effect");
            const ledEffectChanged = () =>
            {
                // Плавность и режим можно выбирать если эффект выбран
                const state = ledEffect.val() == 0;
                ledMode.disabled(state);
                ledModeLabel.setClass("disabled-label", state);
                ledSmooth.disabled(state);
                ledSmoothLabel.setClass("disabled-label", state);
                // Если эффект отсутствует, то только выбранные цвета
                if (state && ledModeUsingRandom())
                    ledMode.val(0).change();
            };
            ledEffect.change(() =>
                {
                    ledEffectChanged();
                    fire();
                });

                
            // Инициализация состояний неонок
            const neonStates = this.dom
                .find(".disp-class-neon-state")
                .toArray()
                .map(e => $(e));
                
            // Подписка на изменение
            neonStates.forEach(e => e.change(fire));
            
            // Плавность неонок
            const neonSmooth = this.dom.find(".disp-class-neon-smooth");
            neonSmooth.on("input", fire);
                
            // Период неонок
            const neonPeriod = this.dom.find(".disp-class-neon-period");
            neonPeriod.on("input", fire);
            
            // Инверсия состояния
            const neonInversion = this.dom.find(".disp-class-neon-inversion");
            neonInversion.change(fire);
            
            // Маски по разрядам неонок
            const neonMasks = [0x01, 0x04, 0x02, 0x08];
               
            // Обработчик оповещения изменения всех полей
            const fieldsChanged = () =>
            {
                ledOneColorChanged();
                ledModeChanged();
                ledEffectChanged();
            };

            fieldsChanged();

            // Чтение данных из пакета
            this.read = data =>
            {
                // Подсветка
                {
                    // Эффект
                    ledEffect.val(data.uint8());
                    
                    // Плавность и режим
                    {
                        const temp = data.uint8();
                        ledSmooth.valSlider(temp & 0x3F); // Младшие 6 бит
                        ledMode.val(temp >> 6); // Старшие 2 бит
                    }
                    
                    // Цвета по разрядам
                    const colorBuffer = new Colors();
                    ledRanks.forEach(rank =>
                        {
                            const g = data.uint8();
                            const r = data.uint8();
                            const b = data.uint8();
                            colorBuffer.setColor({r: r, g: g, b: b}, "rgb");
                            rank.color = colorBuffer;
                        });
                    
                    // Определение и применение режима одного цвета
                    ledOneColor.checked(ledRanks.findIndex(r => r.color.colors.HEX != colorBuffer.colors.HEX) < 0);
                }
                
                // Неонки
                {
                    // Состояния
                    {
                        const mask = data.uint8();
                        for (let i = 0; i < neonStates.length; i++)
                            neonStates[i].checked((mask & neonMasks[i]) != 0);
                    }
                    
                    // Период
                    neonPeriod.valSlider(data.uint8());
                    // Плавность
                    neonSmooth.valSlider(data.uint8());
                    // Инверсия
                    neonInversion.checked(data.bool());
                }

                // Цифры
                {
                    // Эффект
                    digitEffect.val(data.uint8());
                }
                
                fieldsChanged();
            };

            // Запись данных в пакет
            this.write = data =>
            {
                // Подсветка
                {
                    // Эффект
                    data.uint8(ledEffect.val());
                    
                    // Плавность и режим
                    {
                        let temp = ledSmooth.val(); // Младшие 6 бит
                        temp |= ledMode.val() << 6; // Старшие 2 бит
                        data.uint8(temp);
                    }
                    
                    // Цвета по разрядам
                    ledRanks.forEach(rank =>
                        {
                            var rgb = rank.color.colors.RND.rgb;
                            data.uint8(rgb.g);
                            data.uint8(rgb.r);
                            data.uint8(rgb.b);
                        });
                }
                
                // Неонки
                {
                    // Состояния
                    {
                        let mask = 0;
                        for (let i = 0; i < neonStates.length; i++)
                            if (neonStates[i].checked())
                                mask |= neonMasks[i];
                        data.uint8(mask);
                    }
                    
                    // Период
                    data.uint8(neonPeriod.val());
                    // Плавность
                    data.uint8(neonSmooth.val());
                    // Инверсия
                    data.bool(neonInversion.checked());
                }

                // Цифры
                {
                    // Эффект
                    data.uint8(digitEffect.val());
                }
            }
        }
        
        // Класс настроек дисплея с возможность включения
        function DisplaySettingsArm(armCheckBox)
        {
            // Базовый класс
            const base = new DisplaySettings();
            
            // DOM
            this.dom = base.dom;
            
            // Обработчик события изменения
            this.onchange = () => { };
            base.onchange = () => this.onchange();
            
            // Настройка чекбокса включения
            armCheckBox.change(base.onchange);
            
            // Чтение данных из пакета
            this.read = data =>
            {
                base.read(data);
                armCheckBox.checked(data.bool());
            };
            
            // Запись данных в пакет
            this.write = data =>
            {
                base.write(data);
                data.bool(armCheckBox.checked());
            };
        }

        // Класс настроек дисплея с таймаутом
        function DisplaySettingsTimeout(armCheckBox)
        {
            // Базовый класс
            const base = new DisplaySettingsArm(armCheckBox);
            
            // DOM
            const dom = app.page.display.timeoutTemplate();
            this.dom = dom.add(base.dom);
            
            // Обработчик события изменения
            this.onchange = () => { };
            base.onchange = () => this.onchange();
            
            // Настройка слайдера
            dom.setupDurationSlider(20);
            const timeoutSlider = dom.find(".disp-class-timeout-slider");
            timeoutSlider.on("input", base.onchange);
            
            // Чтение данных из пакета
            this.read = data =>
            {
                base.read(data);
                timeoutSlider.valSlider(data.uint8());
            };
            
            // Запись данных в пакет
            this.write = data =>
            {
                base.write(data);
                data.uint8(timeoutSlider.val());
            };
        }
        
        // Класс отправителя пакета при изменениях
        function Packeting(receive, transmit)
        {
            // Обработчик передачи
            let transmitTimeout;
            let transmitLock = false;
            let transmitCount = 0;
            
            // Обработчик передачи
            this.transmit = async () =>
            {
                // Если передача заблокирована
                if (transmitLock)
                    return;
                
                clearTimeout(transmitTimeout);
                if (++transmitCount > 1)
                    return;
                
                // Заполнение пакета
                const packet = new Packet(transmit.code, transmit.name);
                transmit.processing(packet.data);
                
                // Запрос
                await app.session.transmit(packet);

                // Перезапуск таймаута
                if (transmitCount > 1)
                    transmitTimeout = setTimeout(this.transmit);
                transmitCount = 0;
            };
            
            // Обработчик приёма
            this.receive = async () =>
            {
                // Запрос
                const data = await app.session.transmit(new Packet(receive.code, receive.name), loadCounter);
                if (data == null)
                    return;
                
                // Парсинг
                transmitLock = true;
                try
                {
                    receive.processing(data);
                }
                finally
                {
                    transmitLock = false;
                }
            };
        }
        
        // Сцена времени
        const timeScene = new function ()
        {
            // Инициализация сцены
            this.init = () =>
            {
                // Настройки дисплея
                const settings = new DisplaySettings();
                app.dom.disp.time.holder.after(settings.dom);
                
                // Пакетная передача
                const packeting = new Packeting(
                    // Приём
                    {
                        code: app.opcode.STM_DISPLAY_TIME_GET,
                        name: "запрос настроек сцены времени",
                        processing: data => settings.read(data),
                    },
                    // Передача
                    {
                        code: app.opcode.STM_DISPLAY_TIME_SET,
                        name: "применение настроек сцены времени",
                        processing: data => settings.write(data),
                    });
                    
                // Загрузка сцены
                this.load = () => packeting.receive();
                
                // Подписка на события
                settings.onchange = packeting.transmit;
            };
        };
        
        // Настройки яркости
        const lightSettings = new function ()
        {
            // Инициализация
            this.init = () =>
            {
                // Обработчик изменения настроек
                const settingsChanged = () => this.settingsChanged();
                
                // Слайдер ручной подстройки
                const levelManual = app.dom.disp.light.manual.slider;
                app.dom.disp.light.manual.carrier.setupPrecentSlider();
                levelManual.on("input", settingsChanged);
                
                // Ночной режим
                const nightMode = app.dom.disp.light.night;
                nightMode.change(settingsChanged);
                
                // Плавность изменения
                const changeSmooth = app.dom.disp.light.smooth.slider;
                app.dom.disp.light.smooth.carrier.setupDurationSlider();
                changeSmooth.on("input", settingsChanged);
                
                // Автоподстройка яркости
                const levelAuto = app.dom.disp.light.auto;
                const levelAutoChanged = () =>
                {
                    const state = levelAuto.checked();
                    nightMode.disabled(!state);
                    
                    levelManual.disabled(state);
                    app.dom.disp.light.manual.label.setClass("disabled-label", state);
                    
                    changeSmooth.disabled(!state);
                    app.dom.disp.light.smooth.label.setClass("disabled-label", !state);
                };
                levelAutoChanged();
                levelAuto.change(() =>
                {
                    levelAutoChanged();
                    settingsChanged();
                });

                // Пакетная передача
                const packeting = new Packeting(
                    // Приём
                    {
                        code: app.opcode.STM_LIGHT_SETTINGS_GET,
                        name: "запрос настроек освещенности",
                        processing: data =>
                        {
                            levelManual.valSlider(data.uint8());
                            changeSmooth.valSlider(data.uint8());
                            levelAuto.checked(data.bool());
                            nightMode.checked(data.bool());
                        },
                    },
                    // Передача
                    {
                        code: app.opcode.STM_LIGHT_SETTINGS_SET,
                        name: "применение настроек освещенности",
                        processing: data =>
                        {
                            data.uint8(levelManual.val());
                            data.uint8(changeSmooth.val());
                            data.bool(levelAuto.checked());
                            data.bool(nightMode.checked());
                        },
                    });
                
                // Связывание обработчик изменения настроек
                this.settingsChanged = packeting.transmit;
                
                // Периодичное обновление уровня освещенности
                {
                    // Сброс значения надписи текущего состояния
                    const currentStateClear = () => app.dom.disp.light.current.text("");

                    // Таймаут обновления состояния
                    let requestStateTimeout;
                    
                    // Запрос состояния
                    async function requestState()
                    {
                        const data = await app.session.transmit(new Packet(app.opcode.STM_LIGHT_STATE_GET, "получение состояния освещенности"));
                        if (data == null)
                        {
                            currentStateClear();
                            return;
                        }
                        
                        app.dom.disp.light.current.text(data.uint8() + "%");

                        // Перезапрос
                        requestStateTimeout = setTimeout(requestState, 1000);
                    };
                    
                    // Загрузка страницы
                    this.load = () => 
                    {
                        currentStateClear();
                        packeting.receive();
                        requestState()
                    };
                    
                    // Выгрузка страницы
                    this.unload = () => clearTimeout(requestStateTimeout);
                }
            };
        };
        
        // Настройки прогрева ламп
        const heatSettings = new function ()
        {
            // Инициализация
            this.init = () =>
            {
                // Обработчик изменения настроек
                let settingsChanged;
                
                // Инициализация списка часов
                const hour = app.dom.disp.heat.hour;
                utils.range(24).forEach(i =>
                    {
                        let i1 = i + 1;
                        if (i1 >= 24)
                            i1 = 0;
                        const hourText = v => utils.leadingZeros(v, 2) + ":00";
                        utils.dropdownAdd(hour, i, hourText(i) + " — " + hourText(i1));
                    });
                    
                hour.change(() => settingsChanged());

                // Класс информации о типе дня
                function Day(index, shortName, name)
                {
                    this.name = name;
                    this.index = index;
                    this.shortName = shortName;
                }
                    
                // Список типов дней
                const days = 
                    [
                        new Day(0, "ПН", "Понедельник"),    new Day(1, "ВТ", "Вторник"), 
                        new Day(2, "СР", "Среда"),          new Day(3, "ЧТ", "Четверг"), 
                        new Day(4, "ПТ", "Пятница"),        new Day(5, "СБ", "Суббота"), 
                        new Day(6, "ВС", "Воскресенье"), 
                    ];
                
                // Инициализация выбора дня
                {
                    // Максимальное значение индекса списка типов дней
                    const daysIndexMax = days.length - 1;
                    
                    // Обработчик измекнения состония дней
                    const dayStateChanged = () =>
                    {
                        // Список пар диапазонов выбранных дней
                        const pairs = [];
                        
                        // Сброс признака добавления в список выбранных дней
                        days.forEach(d => d.added = false);
                        
                        // Получает признак пропуска обработки дня
                        const skip = i => 
                        {
                            // Пропуск если уже добавлен либо не выбран
                            if (days[i].added || !days[i].state.checked())
                                return true;
                            
                            // Добавляем
                            days[i].added = true;
                            return false;
                        };
                        
                        // Производит спуск в ветку
                        const branch = (i, dx) =>
                        {
                            // Исходный индекс
                            const li = i;
                            
                            // Переход к следующему дню
                            i += dx;
                            if (i < 0)
                                i = daysIndexMax;
                            else if (i > daysIndexMax)
                                i = 0;

                            // Проверка следующего дня
                            return skip(i) ?
                                li :
                                branch(i, dx);
                        };
                        
                        utils.range(days.length).forEach(i =>
                            {
                                // Пропуск если обработан
                                if (skip(i))
                                    return;
                                
                                // Спуск в обе стороны
                                const lf = branch(i, -1);
                                const rf = branch(i, 1);
                                
                                // Если один день
                                if (lf == rf)
                                    pairs.push(days[i].shortName);
                                // Если вся неделя
                                else if (lf == 1 && rf == 0)
                                    pairs.push("Все дни");
                                // Если диапазон
                                else
                                    pairs.push(days[lf].shortName + "-" + days[rf].shortName);
                            });
                            
                        // Если не выбранно ничего
                        if (pairs.length <= 0)
                            pairs.push("Не выбранно");
                        
                        // Конвертирование пар диапазонов дней в текст
                        app.dom.disp.heat.days.button.text(pairs.join(", "));
                    };
                        
                    // Шаблон элемента выпадающего списка дней
                    const dayItemTemplate = app.dom.disp.heat.days.template.makeTemplate("disp-heat-day");
                        
                    // Инициализация списка активации дней
                    const menuHolder = app.dom.disp.heat.days.menu;
                    days.forEach(day =>
                        {
                            // Создание шаблона
                            const item = dayItemTemplate();
                            menuHolder.append(item);
                            
                            // Установка надписи состояния
                            item.find("label").text(day.name);
                            
                            // Подготовка чекбокса состония
                            day.state = item.find("input");
                            day.state.change(() =>
                            {
                                // Оповещение о изменении
                                dayStateChanged();
                                settingsChanged();
                            });
                        });
                        
                    // Предварительное обновление
                    dayStateChanged();
                    
                    // Запрет закрытия всплыващего меню по клику
                    $(menuHolder).on("click", e => e.stopPropagation());
                }
                
                // Инициализация кнопки принудительного запуска
                {
                    const button = app.dom.disp.heat.launch;
                    
                    // Предварительное скрытие спинера
                    button.spinner(false);
                    
                    // Обработчик клика
                    button.click(async () =>
                        {
                            button.spinner(true);
                                await app.session.transmit(new Packet(app.opcode.STM_HEAT_LAUNCH_NOW, "принудительный запуск прогрева ламп"));
                            button.spinner(false);
                        });
                }
                
                // Пакетная передача
                const packeting = new Packeting(
                    // Приём
                    {
                        code: app.opcode.STM_HEAT_SETTINGS_GET,
                        name: "запрос настроек прогрева ламп",
                        processing: data =>
                        {
                            hour.val(data.uint8());
                            let weekDays = data.uint8();
                            days.forEach(day =>
                                {
                                    day.state.checked((weekDays & 1) != 0);
                                    weekDays >>= 1;
                                });
                        },
                    },
                    // Передача
                    {
                        code: app.opcode.STM_HEAT_SETTINGS_SET,
                        name: "применение настроек прогрева ламп",
                        processing: data =>
                        {
                            data.uint8(utils.dropdownSelectedNumber(hour));
                            const weekDays = days.slice().reverse().reduce((value, day) =>
                                {
                                    if (day.state.checked())
                                        value |= 1 << day.index;
                                    
                                    return value;
                                }, 0);
                                
                            data.uint8(weekDays);
                        },
                    });
                
                // Связывание обработчик изменения настроек
                settingsChanged = packeting.transmit;
                
                // Загрузка страницы
                this.load = packeting.receive;
            };
        };            
        
        // Сцена даты
        const dateScene = new function ()
        {
            // Инициализация сцены
            this.init = () =>
            {
                // Настройки дисплея
                const settings = new DisplaySettingsTimeout(app.dom.disp.date.enable);
                app.dom.disp.date.holder.after(settings.dom);
                
                // Пакетная передача
                const packeting = new Packeting(
                    // Приём
                    {
                        code: app.opcode.STM_DISPLAY_DATE_GET,
                        name: "запрос настроек сцены даты",
                        processing: data => settings.read(data),
                    },
                    // Передача
                    {
                        code: app.opcode.STM_DISPLAY_DATE_SET,
                        name: "применение настроек сцены даты",
                        processing: data => settings.write(data),
                    });
                    
                // Загрузка сцены
                this.load = () => packeting.receive();
                
                // Подписка на события
                settings.onchange = packeting.transmit;
            };
        };

        // Сцена адреса своей сети
        const onetScene = new function ()
        {
            // Инициализация сцены
            this.init = () =>
            {
                // Настройки дисплея
                const settings = new DisplaySettingsTimeout(app.dom.disp.onet.enable);
                app.dom.disp.onet.holder.after(settings.dom);
                
                // Пакетная передача
                const packeting = new Packeting(
                    // Приём
                    {
                        code: app.opcode.STM_ONET_SETTINGS_GET,
                        name: "запрос настроек сцены своей сети",
                        processing: data => settings.read(data),
                    },
                    // Передача
                    {
                        code: app.opcode.STM_ONET_SETTINGS_SET,
                        name: "применение настроек сцены своей сети",
                        processing: data => settings.write(data),
                    });
                    
                // Загрузка сцены
                this.load = () => packeting.receive();
                
                // Подписка на события
                settings.onchange = packeting.transmit;
            };
        };

        // Сцена адреса подключенной сети
        const cnetScene = new function ()
        {
            // Инициализация сцены
            this.init = () =>
            {
                // Настройки дисплея
                const settings = new DisplaySettingsTimeout(app.dom.disp.cnet.enable);
                app.dom.disp.cnet.holder.after(settings.dom);
                
                // Пакетная передача
                const packeting = new Packeting(
                    // Приём
                    {
                        code: app.opcode.STM_CNET_SETTINGS_GET,
                        name: "запрос настроек сцены подключенной сети",
                        processing: data => settings.read(data),
                    },
                    // Передача
                    {
                        code: app.opcode.STM_CNET_SETTINGS_SET,
                        name: "применение настроек сцены подключенной сети",
                        processing: data => settings.write(data),
                    });
                    
                // Загрузка сцены
                this.load = () => packeting.receive();
                
                // Подписка на события
                settings.onchange = packeting.transmit;
            };
        };
        
        // Инициализация страницы
        this.init = () =>
        {
            // Шаблон настроек дисплея
            this.mainTemplate = app.dom.disp.template.main.makeTemplate(
                "disp-opts-parent", "disp-opts-lamps",
                "disp-opts-neons", "disp-opts-digit-effect",
                "disp-opts-led-one-color", "disp-opts-led-mode",
                "disp-opts-led-effect", "disp-opts-led-speed",
                "disp-opts-neon-state-lt", "disp-opts-neon-state-rt",
                "disp-opts-neon-state-lb", "disp-opts-neon-state-rb",
                "disp-opts-neon-smooth", "disp-opts-neon-period", 
                "disp-opts-neon-inversion");

            // Шаблон настроек дисплея с опцией таймаута
            this.timeoutTemplate = app.dom.disp.template.timeout.makeTemplate("disp-opts-timeout-slider");
                
            // Инициализация сцен
            timeScene.init();
            dateScene.init();
            onetScene.init();
            cnetScene.init();
            heatSettings.init();
            lightSettings.init();
        };
        
        // Загрузка страницы
        this.loaded = () =>
        {
            loadCounter.reset();
            
            // Загрузка сцен
            timeScene.load();
            dateScene.load();
            onetScene.load();
            cnetScene.load();
            heatSettings.load();
            lightSettings.load();
        };  
        
        // Выгрузка страницы
        this.unloaded = () =>
        {
            // Выгрузка сцен
            lightSettings.unload();
        };

        // Получает, готова ли страница к отображению
        this.ready = () => loadCounter.ready;
    },
};

// Страницы
app.pages = new function ()
{
    // Страницы списком
    const items = 
    [ 
        app.page.time,
        app.page.wifi,
        app.page.display,
    ];
        
    // Инициализация страниц
    this.init = () =>
        items.forEach(page => 
        {
            if (page.init)
                page.init(); 
        });
    
    // Оповещение о загрузке
    this.loaded = () =>
        items.forEach(page => 
        {
            if (page.loaded)
                page.loaded(); 
        });

    // Оповещение о выгрузке
    this.unloaded = () =>
        items.forEach(page =>
        {
            if (page.unloaded)
                page.unloaded(); 
        });
    
    // Возвращает готовы ли все страницы
    this.ready = () => items.find(p => !p.ready()) === undefined;
};

// Сессия
app.session = new function ()
{
    // Фаза начального подключения
    const WS_PHASE_INIT = 0;
    // Фаза нормального приёма
    const WS_PHASE_NORMAL = 1;
    // Фаза восстановления
    const WS_PHASE_RESTORE = 2;

    // Ссылка на сокет
    let ws = null;
    // Режим восстановления сокета
    let wsPhase = WS_PHASE_INIT;
    
    // Контроллер IPC пакетов
    const ipc = new function ()
    {
        // Очередь на отправку
        let queue = [];
        
        // Добавление свойства текущего
        Object.defineProperty(queue, "current", 
            {
                get() 
                {
                    return this.length > 0 ?
                        this[0] :
                        null;
                },
                configurable: false
            });

        // Класс слота очереди 
        function QueueSlot(packet)
        {
            log.assert(packet != undefined);
            
            // Исходный пакет
            this.packet = packet;
            // Массив счетчиков загрузки
            this.loadCounter = [];
            // Массив колбеков результата
            this.resolve = [];
            // Количество повторов
            this.repeat = 0;

            // Признак добавления в очередь
            let pushed = true;
            
            // Удаление из очереди
            const remove = () =>
            {
                log.assert(pushed);
                pushed = false;
                queue.remove(this);
                
            };
            
            // Метод положительного завершения
            this.success = data =>
            {
                remove();
                
                // Вызов цепочки обработчиков
                this.resolve.forEach(cb => cb(data.clone()));
                // Вызов цепочки счетчиков
                this.loadCounter.forEach(lc => lc.increment());
            };
            
            // Метод отрицательного завершения
            this.failed = () => 
            {
                remove();
                
                // Вызов цепочки обработчиков
                this.resolve.forEach(cb => cb(null));
            };
            
            // Добавление в очередь
            queue.push(this);
        }

        // Таймаут на переотправку
        let resendTimeout;
        
        // Очистка таймаута переотправки
        const clearResendTimeout = 
            () => clearTimeout(resendTimeout);
        
        // Сброс очереди (внутренняя функция)
        function reset()
        {
            // Отчистка очереди
            while (queue.current != null)
                queue.current.failed();
            
            // Сброс интервала переотправки
            clearResendTimeout();
        };
        
        // Переотправка текущего пакета
        function resend()
        {
            clearResendTimeout();
            
            // Получаем текущий элемент
            const current = queue.current;
            if (current == null)
                return;
            
            // Проверка истечения количества попыток
            if (++current.repeat > 10)
            {
                // Если режим восстановления
                if (wsPhase != WS_PHASE_RESTORE)
                {
                    // Пробуем снова
                    current.repeat = 0;
                    ws.close();
                    return;
                }
                
                // Не получили данных
                receive();
                return;
            }
            
            // Передача
            try
            {
                ws.send(current.packet.data.toArray());
            }
            catch
            {
                app.session.restart();
                return;
            }
            
            // Установка таймаута
            resendTimeout = setTimeout(resend, 500);
        }
        
        // Передача следующего пакета
        function next()
        {
            const current = queue.current;
            // Если очередь пуста или уже что то отправляется
            if (current == null || current.repeat > 0)
                return;
            
            // Проверяем, есть ли сокет
            if (ws == null)
            {
                reset();
                return;
            }
            
            // Если всё еще в состоянии подключения
            if (ws.readyState != WebSocket.OPEN)
                return;
            
            // Отправялем
            resend();
        };

        // Сброс очереди
        this.reset = reset;

        // Переход к следующему элементу
        this.next = next;
        
        // Передача пакета
        this.transmit = (packet, loadCounter) =>
            new Promise(resolve =>
            {
                // Поиск в очереди пакета с таким же кодом
                let slot = queue.find(s => s.packet.opcode == packet.opcode);
                if (slot == undefined)
                    // Добавление нового слота
                    slot = new QueueSlot(packet);
                
                // Сохранение колбека
                slot.resolve.push(resolve);
                // Добавление опционального счетчика загрузки
                if (loadCounter != undefined)
                    slot.loadCounter.push(loadCounter);
                
                // Пробуем к нему перейти
                next();
            });
        
        // Получение пакета
        function receive(buffer)
        {
            const current = queue.current;
            // Если ничего не отправлялось - выходим
            if (current == null)
                return;
            
            log.assert(current.repeat > 0);
            
            // Если данных нет - ответа не дождались
            if (buffer != null)
            {
                // Получаем данные
                const data = new BinReader(buffer);
                // Чтение команды
                const opcode = data.uint8();
                
                // Проверяем на переотправку
                if (opcode == app.opcode.IPC_RETRY)
                {
                    // Повторяем побыстрее
                    log.error("Received retry command!");
                    resend();
                    return;
                }
                else if (current.packet.opcode != opcode)
                {
                    log.error("Received not our packet " + opcode + ", transmitted " + current.packet.opcode);
                    return;
                }
                
                // Нештяк
                current.success(data);
            }
            else
            {
                current.failed();
                // Вывод сообщения об ошибке
                app.popup.show("Что то пошло не так с запросом: " + current.packet.name + "!");
            }
            
            // Сброс интервала переотправки
            clearResendTimeout();
            // Переход к следующему пакету из очереди
            next();
        };
        
        // Получение пакета
        this.receive = buffer => receive(buffer);
    };

    // Признак загрузки
    let loaded = false;
    // Интервал перезапуска
    let restartTimeout;
    
    // Выгрузка сессии
    this.unload = () =>
    {
        // Сброс интервала перезапуска
        clearTimeout(restartTimeout);
        
        // Оповещение
        if (loaded)
        {
            loaded = false;
            // Сообщения
            app.popup.unloaded();
            // Страниц
            app.pages.unloaded();
            // Дисплея
            app.display.unloaded();
        }
        
        // Отключение WS
        wsPhase = WS_PHASE_INIT;
        if (ws != null)
        {
            const temp = ws;
            ws = null;
            temp.close();
        }
        
        // Сброс IPC
        ipc.reset();
        // Показ прелоадера
        app.overlay.asLoader(true);
    };
    
    // Внутренняя функция перезапуска
    function restart()
    {
        log.info("Socket connecting...");
        // Инициализация WS
        try
        {
            const wsHost = app.debug ?
                // Отладочный адрес
                "192.168.3.2" :
                // Реальный адрес
                window.location.hostname;
            
            ws = new WebSocket("ws://" + wsHost);
        }
        catch
        {
            app.overlay.asError("Ошибка создания WebSocket");
            return;
        }
        // Конфигурирование
        ws.binaryType = "arraybuffer";

        // 10 секунд на подключение
        const connectTimeout = setTimeout(() => ws?.close(), 2 * 1000);
        
        // Обработчик открытия
        ws.onopen = () =>
        {
            clearTimeout(connectTimeout);
            log.info("Socket connected!");
            
            switch (wsPhase)
            {
                case WS_PHASE_INIT:
                    loaded = true;
                    // Оповещение сообщения
                    app.popup.loaded();
                    // Оповещение страниц
                    app.pages.loaded();
                    // Оповещение дисплея
                    app.display.loaded();
                    return;
                    
                case WS_PHASE_NORMAL:
                    log.assert(false);
                    return;
                    
                case WS_PHASE_RESTORE:
                    // Снова переход к следующему
                    log.info("Retransmit packet...");
                    ipc.next();
                    return;
            }
        };
        
        // Обработчик закрытия
        ws.onclose = () => 
        {
            clearTimeout(connectTimeout);
            
            // Если событие уже из выгрузки
            if (ws == null)
                return;

            log.error("Socket disconnected!");

            switch (wsPhase)
            {
                // Если режим восстановления уже включен
                case WS_PHASE_INIT:
                case WS_PHASE_RESTORE:
                    app.session.restart();
                    return;
                    
                case WS_PHASE_NORMAL:
                    // Обработка ниже
                    break;
            }
            
            // Включение режима восстановления
            log.error("Try restore socket...");
            wsPhase = WS_PHASE_RESTORE;
            restart();
        };
        
        // Обработчик получения данных
        ws.onmessage = event =>
        {
            // Сброс восстановления
            switch (wsPhase)
            {
                // Если режим восстановления уже включен
                case WS_PHASE_INIT:
                    log.info("Socket approved!");
                    break;
                    
                case WS_PHASE_RESTORE:
                    log.info("Socket restored!");
                    break;
            }
            wsPhase = WS_PHASE_NORMAL;
            
            // Приём
            ipc.receive(event.data);
            
            // Если основная страница не выводится...
            if (app.dom.container.visible())
                return;
            
            // Показываем главную страницу
            if (app.display.ready() && app.pages.ready())
                app.overlay.asLoader(false);
        };
    };

    // Передача пакета
    this.transmit = ipc.transmit;
    
    // Перезапуск
    this.restart = () =>
    {
        this.unload();
        restartTimeout = setTimeout(restart, 500);
    };
    
    // Инициализация
    this.init = () =>
    {
        // Проверка поддержки WebSockets
        if (window.WebSocket == undefined)
            throw "Нет поддержки WebSocket";
        
        // Перезапуск
        this.restart();
    };
};

// Точка входа
$(() => 
{
    // Инициализация приложения
    try
    {
        // Последовательность не менять
        app.dom.init();
        app.pages.init();
        app.display.init();
        app.session.init();
    } catch (msg)
    {
        // Что то пошло не так
        try
        {
            // Выгрузка
            app.session.unload();
        }
        catch
        { }
        
        // Вывод ошибки инициализации
        try
        {
            app.overlay.asError(msg);
        }
        catch
        {
            // Что бы совсем не потерять лицо
            document.getElementsByTagName("body")[0].style.display = "none";
        }
        throw msg;
    }
});
