// Логирование
var log =
{
    // Вывод в консоль строки с ошибкой
    error: function (msg)
    {
        if (console === undefined || console.error === undefined)
            return;
        console.error(msg);
    },

    // Вывод в консоль строки с информацией
    info: function (msg)
    {
        if (console === undefined || console.info === undefined)
            return;
        console.info(msg);
    },
    
    // Проверка условия
    assert: function (cond, msg)
    {
        if (cond)
            return;
        if (msg === undefined)
            msg = "Assertion error!";
        this.error(msg);
    }
};

// Утилиты по работе с DOM
var utils = new function ()
{
    // Добавление значения к выпа
    this.dropdownAdd = function (dropdown, value, text)
    {
        dropdown.append($("<option />").val(value).text(text));
    };
    
    // Заполнение выпадающего списка числами
    this.dropdownFillNumber = function (dropdown, from, to, ranks)
    {
        // Проверка аргументов
        log.assert(from <= to);
        // Отчистка списка
        dropdown.empty();
        // Заполнение числами
        for (var i = from; i <= to; i++)
        {
            var text = i;
            if (ranks)
                text = this.leadingZeros(text, ranks);
            this.dropdownAdd(dropdown, i, text);
        }
    };
    
    // Заполнение выпадающего списка массивом
    this.dropdownFillArray = function (dropdown, values, texts)
    {
        // Проверка аргументов
        log.assert(values.length == texts.length);
        // Отчистка списка
        dropdown.empty();
        // Заполнение числами
        for (var i = 0; i < values.length; i++)
            this.dropdownAdd(dropdown, values[i], texts[i]);
    };
    
    // Получает значение выбранного элмента в виде числа
    this.dropdownSelectedNumber = function (dropdown)
    {
        return Number(dropdown.val());
    };
    
    // Приводит символы CR LF к одиночному LF
    this.lf = function (input)
    {
        return input.replace('\r', "");
    };
    
    // Добавление лидирующих нулей в числе
    this.leadingZeros = function (value, count)
    {
        value = value.toString();
        while (value.length < count)
            value = "0" + value;
        return value;
    }
};

// Объект цвета
function Color(r, g, b, a)
{
	this.r = r;
	this.g = g;
	this.b = b;
	this.a = a;
    // Для замыканий
    var color = this;
    // Конфертирование в CSS RGBA представление
    this.toCSS = function ()
    {
    	return "rgba(" + 
        	color.r + "," + 
            color.g + "," +
            color.b + "," +
            color.a / 255 + ")";
    };
}

// Диапазон сопосталвения ANSI символов и Unicode
function CpRange(ansiStart, ucsStart, count)
{
    // Инициализация полей
    this.ansi = ansiStart;
    this.ucs = ucsStart;
    this.count = arguments.length > 2 ? count : 1;
    // Проверка аргументов
    log.assert(this.count > 0);
}

// Конвертирование величин
var convert = 
{
    // Кодовые стараницы
    page:
    {
        // Windows-1251
        cp1251:
        [
            new CpRange(0x80, 0x0402, 2),   new CpRange(0x82, 0x201A),      new CpRange(0x83, 0x0453),
            new CpRange(0x84, 0x201E),      new CpRange(0x85, 0x2026),      new CpRange(0x86, 0x2020, 2),
            new CpRange(0x88, 0x20AC),      new CpRange(0x89, 0x2030),      new CpRange(0x8A, 0x0409),
            new CpRange(0x8B, 0x2039),      new CpRange(0x8C, 0x040A),      new CpRange(0x8D, 0x040C),
            new CpRange(0x8E, 0x040B),      new CpRange(0x8F, 0x040F),
            
            new CpRange(0x90, 0x0452),      new CpRange(0x91, 0x2018, 2),   new CpRange(0x93, 0x201C, 2),
            new CpRange(0x95, 0x2022),      new CpRange(0x96, 0x2013, 2),   new CpRange(0x99, 0x2122),
            new CpRange(0x9A, 0x0459),      new CpRange(0x9B, 0x203A),      new CpRange(0x9C, 0x045A),
            new CpRange(0x9D, 0x045C),      new CpRange(0x9E, 0x045B),      new CpRange(0x9F, 0x045F),

            new CpRange(0xA0, 0x00A0),      new CpRange(0xA1, 0x040E),      new CpRange(0xA2, 0x045E),
            new CpRange(0xA3, 0x0408),      new CpRange(0xA4, 0x00A4),      new CpRange(0xA5, 0x0490),
            new CpRange(0xA6, 0x00A6, 2),   new CpRange(0xA8, 0x0401),      new CpRange(0xA9, 0x00A9),
            new CpRange(0xAA, 0x0404),      new CpRange(0xAB, 0x00AB, 4),   new CpRange(0xAF, 0x0407),
            
            new CpRange(0xB0, 0x00B0, 2),   new CpRange(0xB2, 0x0406),      new CpRange(0xB3, 0x0456),
            new CpRange(0xB4, 0x0491),      new CpRange(0xB5, 0x00B5, 3),   new CpRange(0xB8, 0x0451),
            new CpRange(0xB9, 0x2116),      new CpRange(0xBA, 0x0454),      new CpRange(0xBB, 0x00BB),
            new CpRange(0xBC, 0x0458),      new CpRange(0xBD, 0x0405),      new CpRange(0xBE, 0x0455),
            new CpRange(0xBF, 0x0457),      new CpRange(0xC0, 0x0410, 64),
        ],
    },
    
    // Конвертирование одного Unicode символа в Ansi (беззнак)
    ucs2ansi: function (symbol, codePage)
    {
        // Стандарт ANSI
        if (symbol < 0x80)
            return symbol;
        // Поиск в кодовой странице
        for (var i = 0; i < codePage.length; i++)
        {
            var cp = codePage[i];
            if ((symbol >= cp.ucs) && (symbol < (cp.ucs + cp.count)))
                return cp.ansi + symbol - cp.ucs;
        }
        // Неизвестный символ
        return 0x7F;
    },
    
    // Конвертирование одного Ansi (беззнак) символа в Unicode
    ansi2ucs: function (symbol, codePage)
    {
        // Стандарт ANSI
        if (symbol < 0x80)
            return symbol;
        // Поиск в кодовой странице
        for (var i = 0; i < codePage.length; i++)
        {
            var cp = codePage[i];
            if ((symbol >= cp.ansi) && (symbol < (cp.ansi + cp.count)))
                return cp.ucs + symbol - cp.ansi;
        }
        // Неизвестный символ
        return 0x25A1;
    },
    
    // Перевод градусов в радианы
    grad2rad: function (angle)
    {
        return angle * Math.PI / 180;
    },
};

// Класс реализующий чтение бинарных данных из ArrayBuffer
function BinReader(buffer)
{
    var offset = 0;
    var dview = new DataView(buffer);
    
    // Проверка на доступное простаранстров
    function assert(adv)
    {
        log.assert(offset + adv <= buffer.byteLength, "Advance error when reading!");
    };
        
    // Конвертирование значения указанного размера, указанным методом
    function conv(size, method)
    {
        assert(size);
        var result = method.call(dview, offset, true);
        offset += size;
        return result;
    }
    
    // Возвращает, достигнут ли конец потока
    this.eof = function () { return offset >= buffer.byteLength };

    // Чтение Int8
    this.int8 = function () { return conv(1, dview.getInt8) };
    // Чтение UInt8
    this.uint8 = function () { return conv(1, dview.getUint8) };
    // Чтение булевы
    this.bool = function () { return this.uint8() > 0; };
    
    // Чтение Int16
    this.int16 = function () { return conv(2, dview.getInt16) };
    // Чтение UInt16
    this.uint16 = function () { return conv(2, dview.getUint16); };

    // Чтение Int32
    this.int32 = function () { return conv(4, dview.getInt32) };
    // Чтение UInt32
    this.uint32 = function () { return conv(4, dview.getUint32) };
    
    // Чтение нультерминированной Си строки со страницей 1251
    this.cstr = function (chars)
    {
        assert(chars);
        var result = "";
        var skip = false;
        for (var i = 0; i < chars; i++)
        {
            var c = this.uint8();
            // Терминальный символ
            if (skip)
                continue;
            if (c <= 0)
            {
                skip = true;
                continue;
            }
            result += String.fromCharCode(convert.ansi2ucs(c, convert.page.cp1251));
        }
        return result;
    };
}

// Класс реализующий запись бинарных данных в ArrayBuffer
function BinWriter()
{
    // Буфер, содержащий UInt8
    var buffer = [];
    
    // Промежуточный буфер
    var array = new ArrayBuffer(8);
    var dview = new DataView(array);
    var uint8 = new Uint8Array(array);
    
    // Добавление байт из промежуточного в результирующий
    function conv(size, method, x)
    {
        method.call(dview, 0, x, true);
        for (var i = 0; i < size; i++)
            buffer.push(uint8[i]);
    };

    // Запись Int8
    this.int8 = function (x) { conv(1, dview.setInt8, x) };
    // Запись UInt8
    this.uint8 = function (x) { conv(1, dview.setUint8, x) };
    // Запись булевы
    this.bool = function (x) { this.uint8(x ? 1 : 0); };
    
    // Запись Int16
    this.int16 = function (x) { conv(2, dview.setInt16, x) };
    // Запись UInt16
    this.uint16 = function (x) { conv(2, dview.setUint16, x) };

    // Запись Int32
    this.int32 = function (x) { conv(4, dview.setInt32, x) };
    // Запись UInt32
    this.uint32 = function (x) { conv(4, dview.setUint32, x) };
    
    // Запись нультерминированной Си строки со страницей 1251
    this.cstr = function (str, chars)
    {
        log.assert(str.length < chars);
        // Обход символов
        for (var i = 0; i < str.length; i++, chars--)
            this.uint8(convert.ucs2ansi(str.charCodeAt(i), convert.page.cp1251));
        // Терминальные символы
        for (; chars > 0; chars--)
            this.uint8(0);
    };

    // Конфвертирование ArrayBuffer
    this.toArray = function ()
    {
        var result = new ArrayBuffer(buffer.length);
        var u8 = new Uint8Array(result);
        for (var i = 0; i < buffer.length; i++)
            u8[i] = buffer[i];
        return result;
    };
}

// Расширения для jQuery
jQuery.fn.extend(
{
    // Показывает/скрывает элемент по булвому значению
    visible: function (state) 
    {
        if (state)
            this.show();
        else
            this.hide();
    },
    
    // Установка/снятие/получение состояния чекбокса
    checked: function (state)
    {
        if (state === undefined)
            return this.is(":checked");
        this.prop("checked", state);
    },

    // Установка/снятие/получение разрешения клика по кнопке
    disabled: function (state)
    {
        if (state === undefined)
            return this.prop("disabled");
        if (state)
            this.prop("disabled", true);
        else
            this.removeAttr("disabled");
    },
    
    // Показывает/скрывает спинер на кнопке
    spinner: function (state)
    {
        this.children("span").css("opacity", state ? 0 : 1);
        this.children("div.spinner-border").visible(state);
    },
});
