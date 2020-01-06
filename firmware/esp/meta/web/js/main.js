var app = { };

// Коды команд
app.opcode =
{
    // Код ошибки для переотправки
    IPC_RETRY: 0x00,
    // Запрос текущей даты/времени
    STM_TIME_GET: 0x02,
    // Установка текущей даты/времени
    STM_TIME_SET: 0x03,
    // Синхронизация даты/времени
    STM_TIME_SYNC_START: 0x04,
    
    // Запрос настроек даты/времени
    STM_TIME_SETTINGS_GET: 0x05,
    // Установка настроек даты/времени
    STM_TIME_SETTINGS_SET: 0x06,
    
    // Запрос настроек WiFi
    STM_WIFI_SETTINGS_GET: 0x07,
    // Установка настроек WiFi
    STM_WIFI_SETTINGS_SET: 0x08,

    // Оповещение, что настройки WiFi сменились
    ESP_WIFI_SETTINGS_CHANGED: 0x65,
    
    // Запрос даты/времени из интернета
    ESP_TIME_SYNC: 0x66,
    // Передача списка хостов SNTP
    ESP_TIME_HOSTLIST_SET: 0x67,
};

// Класс исходящего пакета
function OutPacket(opcode, name)
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

// Класс входящего пакета
function InPacket(buffer)
{
    // Инициализация полей
    this.data = new BinReader(buffer);
    // Чтение команды
    this.opcode = this.data.uint8();
}

// Оверлей
app.overlay = new function ()
{
    var error = "error";
    
    // Показывает/скрывает панель оверлея
    var toggle = function (show)
    {
        app.dom.container.visible(!show);
        app.dom.overlay.container.visible(show);
    };
    
    // Показать/скрыть как обычная загрузка
    this.asLoader = function (show)
    {
        app.dom.overlay.container.removeClass(error);
        toggle(show);
    };
    
    // Показать как фатальную ошибку
    this.asError = function (msg)
    {
        app.dom.overlay.container.addClass(error);
        toggle(true);
        if (msg === undefined)
            return;
        app.dom.overlay.message.text(msg);
    };
};

// Всплывающее окно
app.popup = new function ()
{
    // Показ всплывающего окна
    this.show = function (msg)
    {
        if (msg === undefined)
            return;
        app.dom.popup.message.text(msg);
        app.dom.popup.container.modal();
    };
};

// Список DOM элементов
app.dom = new function ()
{
    // Флаг ошибки
    var error = false;
    
    // Внутренняя функция ковертирования карты в DOM элементы
    function convert(dom, map)
    {
        for (var prop in map) 
        {
            var value = map[prop];
            if (typeof value == "object")
            {
                var obj = { };
                dom[prop] = obj
                convert(obj, value);
                continue;
            }
            if (typeof value == 'string')
            {
                var obj = $(value);
                if (obj.length <= 0)
                    error = true;
                dom[prop] = obj
                continue;
            }
            error = true;
        }
    };
    
    // Карта селекторов
    var map = 
    {
        nixie: "#nixie-display",
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
        
        wifiAp:
        {
            ssid: "#wifi-ap-ssid",
            pass: "#wifi-ap-pass"
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
    this.init = function ()
    {
        convert(this, map);
        if (error)
            throw "Ошибка инициализации макета";
    };
};

// Объект дисплея неоновых ламп
app.nixie = new function ()
{
    var canvas;
    var context;
    var self = this;
    // Инициализация неоновых ламп
    this.tubes = [];
    for (var i = 0; i < 6; i++)
        this.tubes.push(new NixieTube());
    
    // Обновление дисплея
    this.redraw = function ()
    {
        // Отчистка экрана
        context.fillRect(0, 0, canvas.width, canvas.height);
        // Вывод неоновых ламп
        context.save();
            context.translate(20, 5);
            for (var i = 0; i < self.tubes.length; i++)
            {
                self.tubes[i].draw(context);
                context.translate(160 + ((i % 2 != 0) ? 25 : 0), 0);
            }
        context.restore();
    };

    // Установка цифры на определнном разряде [0..2]
    this.setNumber = function (rank, value)
    {
        // Проверка аргументов
        log.assert(rank >= 0 && rank <= 2);
        log.assert(value >= 0 && value <= 99);
        // Обновление
        this.tubes[rank * 2 + 0].digit = Math.floor(value / 10);
        this.tubes[rank * 2 + 1].digit = Math.floor(value % 10);
    };
    
    // Инициализация
    this.init = function ()
    {
        var error = "Нет поддержки HTML5 Canvas";
        // Получаем элемент Canvas
        canvas = app.dom.nixie[0];
        // Получаем метод
        if (canvas.getContext === undefined)
            throw error;
        // Получаем контекст
        context = canvas.getContext("2d");
        if (context == null)
            throw error;
        // Начальная настройка канвы
        context.fillStyle = "white";
        // Старт цикла обновления дисплея
        setInterval(this.redraw, 500);
    };  
};

// Страницы
app.page = 
{
    // Страница даты/времени
    time: new function ()
    {
        // Счетчик загруженных пакетов
        var loadedCount = 0;
        // Время синхронизации с часами
        var syncTime = new Date();
        // Время прочтенное с часов
        var readedTime = new Date();
        // Время с завершения изменения даты/времени
        var modifyTime = new Date(0);
        
        // Обработчик входящих пакетов
        var receiveHandler = function (transmited, received)
        {
            var data;
            var fail = received == null;
            if (!fail)
            {
                loadedCount++;
                var data = received.data;
            }
            switch (transmited.opcode)
            {
                case app.opcode.STM_TIME_GET:
                    if (fail)
                        break;
                    syncTime = new Date();
                    // Декодирование
                    {
                        // Текущая дата
                        var year = data.uint8();
                        var month = data.uint8();
                        var day = data.uint8();
                        var hour = data.uint8();
                        var minute = data.uint8();
                        var second = data.uint8();
                        readedTime = new Date(year + 2000, month - 1, day, hour, minute, second);
                        
                        // Дата синхронизации
                        year = data.uint8();
                        month = data.uint8();
                        day = data.uint8();
                        hour = data.uint8();
                        minute = data.uint8();
                        second = data.uint8();
                        
                        // Вывод времени синхронизации
                        var notice = app.dom.time.ntp.notice;
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
                    log.info("Received current datetime: " + readedTime + "...");
                    updateTimeOnDom();
                    break;
                    
                case app.opcode.STM_TIME_SET:
                    if (fail)
                        modifyTime = new Date(0);
                    // Перезапрашивает текущую дату/время
                    requestTime();
                    break;
                    
                case app.opcode.STM_TIME_SETTINGS_GET:
                    if (fail)
                        break;
                    // Декодирование
                    {
                        app.dom.time.ntp.enable.checked(data.bool());
                        app.dom.time.ntp.timezone.val(data.int8());
                        app.dom.time.ntp.offset.val(data.int8());
                        app.dom.time.ntp.list.val(data.cstr(260));
                    }
                    log.info("Received datetime sync settings...");
                    break;
                
                case app.opcode.STM_TIME_SETTINGS_SET:
                    // Перезапрашивает текущие настройки и дату/время
                    requestTime();
                    requestSettings();
                    break;
                    
                case app.opcode.STM_TIME_SYNC_START:
                    if (fail)
                    {
                        synchronizer.feedback(-1);
                        break;
                    }
                    synchronizer.feedback(data.uint8());
                    break;
            }
        };
        
        // Запрос текущей даты/времени
        var requestTime = function ()
        {
            app.session.transmit(new OutPacket(app.opcode.STM_TIME_GET, "запрос даты/времени"), receiveHandler);
        };

        // Запрос текущей даты/времени
        var requestSettings = function ()
        {
            app.dom.time.ntp.apply.spinner(false);
            app.session.transmit(new OutPacket(app.opcode.STM_TIME_SETTINGS_GET, "запрос настроек синхронизации"), receiveHandler);
        };
        
        // Обновление текущего времени на дисплее
        var updateTimeOnDom = function ()
        {
            var time = app.page.time.currentTime();
            // Год
            var yy = time.getFullYear();
            // Месяц
            var nn = time.getMonth();
            // День
            var dd = time.getDate();
            // Часы
            var hh = time.getHours();
            app.nixie.setNumber(0, hh);
            // Минуты
            var mm = time.getMinutes();
            app.nixie.setNumber(1, mm);
            // Секунды
            var ss = time.getSeconds();
            app.nixie.setNumber(2, ss);
            
            // Можем ли модифицировать поля ввода
            var dx = new Date() - modifyTime;
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
        var synchronizer = new function ()
        {
            // Флаг состояния работы
            var running = false;
            
            // Передача запроса
            var transmit = function (check)
            {
                log.assert(running);
                // Отправляем пакет с установкой даты/времени
                var packet = new OutPacket(app.opcode.STM_TIME_SYNC_START, "синхронизация даты/времени");
                packet.data.bool(check);
                app.session.transmit(packet, receiveHandler);
            };
            
            // Запуск автомата
            this.run = function ()
            {
                if (running)
                    return;
                running = true;
                transmit();
                app.dom.time.ntp.sync.spinner(true);
                log.info("Start date/time syncronization...");
            };
            
            // Сброс автомата
            this.reset = function ()
            {
                running = false;
                app.dom.time.ntp.sync.spinner(false);
            };
            
            // Обратная связь с полученного ответа
            this.feedback = function (status)
            {
                if (!running)
                    return;
                switch (status)
                {
                    // Ошибка сети
                    case -1:
                        break;
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
                        transmit(true);
                        log.info("Date/time syncronization pending...");
                        return;
                }
                this.reset();
            };
        };

        // Инициализация страницы
        this.init = function ()
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
                .add(app.dom.time.year).on('change', function ()
            {
                modifyTime = new Date();
                // Отправляем пакет с установкой даты/времени
                var packet = new OutPacket(app.opcode.STM_TIME_SET, "применение даты/времени");
                packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.year) - 2000);
                packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.month) + 1);
                packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.day));
                packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.hour));
                packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.minute));
                packet.data.uint8(utils.dropdownSelectedNumber(app.dom.time.second));
                app.session.transmit(packet, receiveHandler);
            });
            // Обновление времени на дисплее
            setInterval(updateTimeOnDom, 500);

            // Обработчик клика по кнопке применения настроек
            app.dom.time.ntp.apply.click(function () 
            {
                app.dom.time.ntp.apply.spinner(true);
                // Отправляем пакет с установкой даты/времени
                var packet = new OutPacket(app.opcode.STM_TIME_SETTINGS_SET, "применение настроек синхронизации");
                packet.data.bool(app.dom.time.ntp.enable.checked());
                packet.data.int8(app.dom.time.ntp.timezone.val());
                packet.data.int8(app.dom.time.ntp.offset.val());
                packet.data.cstr(utils.lf(app.dom.time.ntp.list.val().trim()), 260);
                app.session.transmit(packet, receiveHandler);
            });
            
            // Обработчик клика по кнопке синхронизации даты/времени
            app.dom.time.ntp.sync.click(function () 
            {
                synchronizer.run();
            });
        };
        
        // Перезагрузка страницы
        this.reload = function ()
        {
            loadedCount = 0;
            // Запрос даты/времени и настроек
            requestTime();
            requestSettings();
            // Сброс автомата синхронизации
            synchronizer.reset();
        };
        
        // Получает, готова ли страница к отображению
        this.ready = function ()
        {
            // Нужно 2 (текущая дата/время и настройки)
            return loadedCount >= 2;
        };
        
        // Получает текущую дату/время
        this.currentTime = function ()
        {
            var dx = new Date();
            dx -= syncTime;
            return new Date(readedTime.getTime() + dx);
        };
    },
    
    // Страница настроек WiFi
    wifi: new function ()
    {
        // Счетчик загруженных пакетов
        var loadedCount = 0;
        
        // Инициализация страницы
        this.init = function ()
        {
            
        };
        
        // Перезагрузка страницы
        this.reload = function ()
        {
            loadedCount = 0;
            
        };
        
        // Получает, готова ли страница к отображению
        this.ready = function ()
        {
            // Нужно 1 (только настройки)
            return loadedCount >= 0;
        };
    },
};

// Страницы
app.pages = new function ()
{
    // Страницы списком
    var items = 
    [ 
        app.page.time,
        app.page.wifi,
    ];
    
    // Обход страниц
    this.forEach = function (fn)
    {
        items.forEach(fn);
    };
    
    // Инициализация страниц
    this.init = function ()
    {
        this.forEach(function (page) { page.init(); });
    };
    
    // Перезагрузка страниц
    this.reload = function ()
    {
        this.forEach(function (page) { page.reload(); });
    };
    
    // Возвращает готовы ли все страницы
    this.ready = function ()
    {
        for (var i = 0; i < items.length; i++)
            if (!items[i].ready())
                return false;
        return true;
    };
};

// Сессия
app.session = new function ()
{
    // Ссылка на WS
    var ws = null;
    // Дескриптор интервала перезапуска
    var restartHandle;
    
    // Контроллер IPC пакетов
    var ipc = new function ()
    {
        // Очередь на отправку
        var queue = [];
        // Текущий отправляемый пакет
        var sended = null;
        // Таймаут на переотправку
        var resendHandle;
        // Время начала передачи запроса
        var resendTime = new Date(0);
        
        // Сброс очереди (внутренняя функция)
        var reset = function ()
        {
            queue = [];
            sended = null;
            // Сброс интервала переотправки
            clearInterval(resendHandle);
        };
        
        // Установка таймаута переотправки
        var setResendTimeout = function (mills)
        {
            if (mills === undefined)
                mills = 500;
            resendHandle = setTimeout(resend, mills);
        };
        
        // Переотправка текущего пакета
        var resend = function ()
        {
            if (sended == null)
                return;
            // Проверка истечения времени
            if (new Date() - resendTime > 3000)
            {
                receive();
                return;
            }
            
            // Передача
            try
            {
                ws.send(sended.packet.data.toArray());
            }
            catch (e)
            {
                app.session.restart();
                return;
            }
            
            // Установка таймаута
            setResendTimeout();
        }
        
        // Передача следующего пакета
        var next = function ()
        {
            // Если очередь пуста или уже что то отправляется
            if (sended != null || queue.length <= 0)
                return;
            // Проверяем, есть ли сокет
            if (ws == null)
            {
                reset();
                return;
            }
            // Получаем первый элемент
            sended = queue.shift();
            // Текущее время
            resendTime = new Date();
            // Отправка
            resend();
        };

        // Сброс очереди
        this.reset = function () { reset() };
        
        // Передача пакета
        this.transmit = function (packet, ontransmit)
        {
            if (ontransmit === undefined)
                ontransmit = null;
            // Поиск в очереди пакета с таким же кодом
            for (var i = 0; i < queue.length; i++)
                if (queue[i].packet.opcode == packet.opcode)
                {
                    // Замена
                    queue[i].packet = packet;
                    queue[i].ontransmit = ontransmit;
                    return;
                }
            // Добавление в конец
            queue.push({ packet: packet, ontransmit: ontransmit });
            // Пробуем к нему перейти
            next();
        };
        
        // Получение пакета
        var receive = function (buffer)
        {
            // Если ничего не отправлялось - выходим
            if (sended == null)
                return;
            
            // Парсинг пакета
            var packet;
            // Если данных нет - ответа не дождались
            if (buffer != null)
            {
                packet = new InPacket(buffer);
                // Проверяем на переотправку
                if (packet.opcode == app.opcode.IPC_RETRY)
                {
                    // Повторяем побыстрее
                    log.error("Received retry command!");
                    setResendTimeout(100);
                    resend();
                    return;
                }
                else if (sended.packet.opcode != packet.opcode)
                {
                    log.error("Received not our packet " + packet.opcode + ", transmitted " + sended.packet.opcode);
                    return;
                }
            }
            else
            {
                packet = null;
                // Вывод сообщения об ошибке
                app.popup.show("Что то пошло не так с запросом: " + sended.packet.name + "!");
            }
            // Сброс интервала переотправки
            clearInterval(resendHandle);
            // Оповещение
            var handler = sended.ontransmit;
            if (handler != null)
                handler(sended.packet, packet);
            // Переход к следующему пакету из очереди
            sended = null;
            next();
        };
        
        // Получение пакета
        this.receive = function (buffer) { receive(buffer) };
    };
    
    // Выгрузка сессии
    this.unload = function ()
    {
        // Сброс интервала перезапуска
        clearInterval(restartHandle);
        // Отключение WS
        if (ws != null)
            ws.close();
        ws = null;
        // Сброс IPC
        ipc.reset();
        // Показ прелоадера
        app.overlay.asLoader(true);
    };
    
    // Внутренняя функция перезапуска
    function restart()
    {
        // Инициализация WS
        try
        {
            // TODO: вернуть ws = new WebSocket("ws://" + window.location.hostname + ":7777");
            ws = new WebSocket("ws://192.168.88.81:80");
        }
        catch (e)
        {
            app.overlay.asError("Ошибка создания WebSocket");
            return;
        }
        // Конфигурирование
        ws.binaryType = "arraybuffer";

        // Обработчик открытия
        ws.onopen = function (event)
        {
            // Оповещение всех страниц
            app.pages.reload();
        };
        
        // Обработчик закрытия
        ws.onclose = function (evt)
        {
            app.session.restart();
        };
        
        // Обработчик получения данных
        ws.onmessage = function (evt)
        {
            ipc.receive(evt.data);
            // Если основная страница не выводится...
            if (app.dom.container.is(":visible"))
                return;
            // Показываем главную страницу
            if (app.pages.ready())
                app.overlay.asLoader(false);
        };
    };

    // Передача пакета
    this.transmit = function (packet, ontransmit)
    {
        return ipc.transmit(packet, ontransmit);
    };
    
    // Перезапуск
    this.restart = function ()
    {
        this.unload();
        restartHandle = setTimeout(restart, 500);
    };
    
    // Инициализация
    this.init = function ()
    {
        // Проверка поддержки WebSockets
        if (window.WebSocket === undefined)
            throw "Нет поддержки WebSocket";
        
        // Перезапуск
        this.restart();
    };
};

// Точка входа
$(function () 
{
    // Инициализация приложения
    try
    {
        // Последовательность не менять
        app.dom.init();
        app.pages.init();
        app.nixie.init();
        app.session.init();
    } catch (msg)
    {
        // Что то пошло не так
        try
        {
            // Выгрузка
            app.session.unload();
        }
        catch (e)
        { }
        
        // Вывод ошибки инициализации
        try
        {
            app.overlay.asError(msg);
        }
        catch (e)
        {
            // Что бы совсем не потерять лицо
            document.getElementsByTagName("body")[0].style.display = "none";
        }
        throw msg;
    }
});
