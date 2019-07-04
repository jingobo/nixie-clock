var app = { };

// Оверлей
app.overlay = new function ()
{
    var error = "error";
    
    // Показывает/скрывает панель оверлея
    var toggle = function (show)
    {
        app.dom.overlay.visible(show);
        app.dom.container.visible(!show);
    };
    
    // Показать/скрыть как обычная загрузка
    this.asLoader = function (show)
    {
        app.dom.overlay.removeClass(error);
        toggle(show);
    };
    
    // Показать как фатальную ошибку
    this.asError = function (msg)
    {
        app.dom.overlay.addClass(error);
        toggle(true);
        if (msg === undefined)
            return;
        app.dom.overlay.find(".text-danger").text(msg);
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
        overlay: "#overlay",
        container: ".container",
        
        wifiAp:
        {
            ssid: "#wifi-ap-ssid",
            pass: "#wifi-ap-pass"
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

// Сессия
app.session = new function ()
{
    // Ссылка на WS
    var ws = null;
    // Дескриптор интервала перезапуска
    var restartHandle;
    
    // Выгрузка сессии
    function unload()
    {
        // Сброс интервала перезапуска
        clearInterval(restartHandle);
        // Отключение WS
        if (ws != null)
            ws.close();
        ws = null;
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
            ws = new WebSocket("ws://192.168.88.92:80");
        }
        catch (e)
        {
            app.overlay.asError("Ошибка создания WebSocket");
            return;
        }
        // Конфигурирование
        ws.binaryType = "arraybuffer";
        
        // Обработчик открытия
        ws.onopen = function(event)
        {
            // Показ приложения
            // TODO: вернууть app.overlay.asLoader(false);

            var writer = new BinWriter();
            writer.uint8(0x02);
            writer.int8(0xAA);
            
            ws.send(writer.toArray());
        };
        
        // Обработчик закрытия
        ws.onclose = function(evt)
        {
            // TODO: app.session.restart();
            app.overlay.asLoader(false);
        };
        
        // Обработчик получения данных
        ws.onmessage = function(evt)
        {
            var reader = new BinReader(evt.data);
            var a = reader.uint8();
            var b = reader.int8();
            var c = reader.uint32();
            var d = reader.int8();
            var e = reader.uint8();
            var st = reader.cstr(18);
            
            var writer = new BinWriter();
            writer.uint8(255);
            writer.int8(-100);
            writer.uint32(0xAABBCCDD);
            writer.cstr("Hello world! Ёба)", 20);
            
            var ar = writer.toArray();
            //ws.send(ar);
        };
    };
        
    // Перезапуск
    this.restart = function ()
    {
        unload();
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
        app.dom.init();
        app.nixie.init();
        app.session.init();
    } catch (msg)
    {
        app.overlay.asError(msg);
    }
});