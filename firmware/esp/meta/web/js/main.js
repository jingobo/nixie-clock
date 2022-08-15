const app = 
{
    // Признак отладки
    debug: window.location.protocol == 'file:',
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
    const nixies = range(nixieCount).map(() =>
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
    const neons = range(neonCount).map(() =>
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
    
    // Запрос состояния
    async function request()
    {
        const data = await app.session.transmit(new Packet(app.opcode.STM_SCREEN_STATE_GET, "получение состояния экрана"));
        if (data == null)
            return;
        
        // Данные ламп
        nixies.forEach(tube =>
            {
                tube.digit = data.uint8();
                tube.satSmooth.start(data.uint8());
                tube.dot = data.bool();
            });

        // Данные подсветки
        nixies.forEach(tube =>
            {
                const r = data.uint8();
                const g = data.uint8();
                const b = data.uint8();
                
                tube.led.rSmooth.start(r);
                tube.led.gSmooth.start(g);
                tube.led.bSmooth.start(b);
                
                tube.led.aSmooth.start(Math.max(r, g, b) * 0.65);
            });
        
        // Данные неонок
        neons.forEach(neon => neon.satSmooth.start(data.uint8() > 0 ? 255 : 0));
        
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
};

// Страницы
app.page = 
{
    // Страница даты/времени
    time: new function ()
    {
        // Счетчик загруженных пакетов
        const loadCounter = new LoadCounter(2);
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
            let year = data.uint8();
            let month = data.uint8();
            let day = data.uint8();
            let hour = data.uint8();
            let minute = data.uint8();
            let second = data.uint8();
            readedTime = new Date(year + 2000, month - 1, day, hour, minute, second);
            
            // Дата синхронизации
            year = data.uint8();
            month = data.uint8();
            day = data.uint8();
            hour = data.uint8();
            minute = data.uint8();
            second = data.uint8();
            // Возможность синхронизации
            app.dom.time.ntp.sync.disabled(!data.bool());
            
            // Вывод времени синхронизации
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
            const time = app.page.time.currentTime();
            // Год
            const yy = time.getFullYear();
            // Месяц
            const nn = time.getMonth();
            // День
            const dd = time.getDate();
            // Часы
            const hh = time.getHours();
            // Минуты
            const mm = time.getMinutes();
            // Секунды
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
        
        // Получает признак загрузки настроек
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
        
        // Счетчик загруженных пакетов
        const loadCounter = new LoadCounter(2);
                
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
            stations = app.dom.wifi.sta.list.templatedList("wifi-sta-info", "wifi-sta-pass");
            
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
            searcher.reset();
            requestSettings();
            requestState();
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
        // Инициализация страницы
        this.init = () =>
        {
            $('.color-picker').colorPicker();
        };        
        
        // Загрузка страницы
        this.loaded = () =>
        {
            
        };  

        // Загрузка страницы
        this.loaded = () =>
        {
            
        };  
        
        // Выгрузка страницы
        this.unloaded = () =>
        {
            
        };

        // Получает, готова ли страница к отображению
        this.ready = () => true;
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
        ws.onopen = event =>
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
        ws.onclose = event => 
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
            if (app.pages.ready())
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
