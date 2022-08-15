// Логирование
const log =
{
    // Вывод в консоль строки с ошибкой
    error(msg)
    {
        if (console === undefined)
            return;
        console?.error(msg);
    },

    // Вывод в консоль строки с информацией
    info(msg)
    {
        if (console === undefined)
            return;
        console?.info(msg);
    },
    
    // Проверка условия
    assert(cond, msg)
    {
        if (cond)
            return;
        
        if (msg === undefined)
            msg = "Assertion error!";
        this.error(msg);
    }
};

// Утилиты по работе с DOM
const utils =
{
    // Добавление значения к выпа
    dropdownAdd(dropdown, value, text)
    {
        dropdown.append($("<option />").val(value).text(text));
    },
    
    // Заполнение выпадающего списка числами
    dropdownFillNumber(dropdown, from, to, ranks)
    {
        // Проверка аргументов
        log.assert(from <= to);
        // Отчистка списка
        dropdown.empty();
        // Заполнение числами
        for (let i = from; i <= to; i++)
        {
            let text = i;
            if (ranks)
                text = this.leadingZeros(text, ranks);
            this.dropdownAdd(dropdown, i, text);
        }
    },
    
    // Заполнение выпадающего списка массивом
    dropdownFillArray(dropdown, values, texts)
    {
        // Проверка аргументов
        log.assert(values.length == texts.length);
        // Отчистка списка
        dropdown.empty();
        // Заполнение числами
        for (let i = 0; i < values.length; i++)
            this.dropdownAdd(dropdown, values[i], texts[i]);
    },
    
    // Получает значение выбранного элмента в виде числа
    dropdownSelectedNumber(dropdown)
    {
        return Number(dropdown.val());
    },
    
    // Приводит символы CR LF к одиночному LF
    lf(input)
    {
        return input.replace('\r', "");
    },
    
    // Добавление лидирующих нулей в числе
    leadingZeros(value, count)
    {
        value = value.toString();
        while (value.length < count)
            value = "0" + value;
        return value;
    }
};

// Кодирование текста
const textCoder = new function ()
{
    // Декодирование
    const decoder = new TextDecoder();
    
    this.decode = array => decoder.decode(new Uint8Array(array));
    
    // Кодирование
    const encoder = new TextEncoder();
    
    this.encode = str => Array.from(encoder.encode(str));
};

// Генератор диапазона
function range(from, to)
{
    const result = [];
    for (let i = from; i <= to; i++)
        result.push(i);
    
    return result;
}

// Генератор индексов
function range(count)
{
    const result = [];
    for (let i = 0; i < count; i++)
        result.push(i);
    
    return result;
}

// Класс цвета
function Color(r, g, b, a)
{
    this.r = r;
    this.g = g;
    this.b = b;
    this.a = a;
    
    // Конфертирование в CSS RGBA представление
    this.toCSS = function ()
    {
        return "rgba(" + 
            this.r + "," + 
            this.g + "," +
            this.b + "," +
            this.a / 255 + ")";
    };
}

// Класс счетчика загрузок
function LoadCounter(maximum)
{
    // Максимум опционален
    if (maximum === undefined)
        maximum = 0;
    
    // Счетчик
    let counter = 0;
    
    // Добавление свойства количества
    Object.defineProperty(this, 'count', 
        {
            get() { return counter; },
            configurable: false
        });
        
    // Добавление свойства готовности
    Object.defineProperty(this, 'ready', 
        {
            get() { return counter >= maximum; },
            configurable: false
        });
    
    // Сброс счетчика
    this.reset = () => counter = 0;
    // Инкремент счетчика
    this.increment = () => counter++;
}

// Класс реализующий чтение бинарных данных из ArrayBuffer
function BinReader(buffer, offset)
{
    if (offset == undefined)
        offset = 0;
    const dview = new DataView(buffer);
    
    // Проверка на доступное простаранстров
    function assert(adv)
    {
        log.assert(offset + adv <= buffer.byteLength, "Advance error when reading!");
    };
        
    // Конвертирование значения указанного размера, указанным методом
    function conv(size, method)
    {
        assert(size);
        const result = method.call(dview, offset, true);
        offset += size;
        return result;
    }
    
    // Возвращает, достигнут ли конец потока
    this.eof = () => offset >= buffer.byteLength;

    // Чтение Int8
    this.int8 = () => conv(1, dview.getInt8);
    // Чтение UInt8
    this.uint8 = () => conv(1, dview.getUint8);
    // Чтение булевы
    this.bool = () => this.uint8() > 0;
    
    // Чтение Int16
    this.int16 = () => conv(2, dview.getInt16);
    // Чтение UInt16
    this.uint16 = () => conv(2, dview.getUint16);

    // Чтение Int32
    this.int32 = () => conv(4, dview.getInt32);
    // Чтение UInt32
    this.uint32 = () => conv(4, dview.getUint32);
    
    // Чтение нультерминированной Си строки
    this.cstr = chars =>
    {
        assert(chars);
        
        // Получаем байты строки
        const source = [];
        {
            let skip = false;
            for (let i = 0; i < chars; i++)
            {
                // Получаем текущий символ
                const c = this.uint8();
                
                // Если конец строки
                if (skip)
                    continue;
                
                // Терминальный символ
                if (c <= 0)
                {
                    skip = true;
                    continue;
                }
                
                source.push(c);
            }
            
            log.assert(skip, "Null termination symbol not found");
        }
        
        // Декодирование
        return textCoder.decode(source);
    };
    
    // Клонирование
    this.clone = () => new BinReader(buffer, offset);
}

// Класс реализующий запись бинарных данных в ArrayBuffer
function BinWriter()
{
    // Буфер, содержащий UInt8
    const buffer = [];
    
    // Промежуточный буфер
    const array = new ArrayBuffer(8);
    const dview = new DataView(array);
    const uint8 = new Uint8Array(array);
    
    // Добавление байт из промежуточного в результирующий
    function conv(size, method, x)
    {
        method.call(dview, 0, x, true);
        for (let i = 0; i < size; i++)
            buffer.push(uint8[i]);
    };

    // Запись Int8
    this.int8 = x => conv(1, dview.setInt8, x);
    // Запись UInt8
    this.uint8 = x => conv(1, dview.setUint8, x);
    // Запись булевы
    this.bool = x => this.uint8(x ? 1 : 0);
    
    // Запись Int16
    this.int16 = x => conv(2, dview.setInt16, x);
    // Запись UInt16
    this.uint16 = x => conv(2, dview.setUint16, x);

    // Запись Int32
    this.int32 = x => conv(4, dview.setInt32, x);
    // Запись UInt32
    this.uint32 = x => conv(4, dview.setUint32, x);
    
    // Запись нультерминированной Си строки со страницей 1251
    this.cstr = (str, chars) =>
    {
        // Обход символов
        const data = textCoder.encode(str);
        data.forEach(this.uint8);
        
        chars -= data.length;
        log.assert(chars > 0);

        // Терминальные символы
        for (; chars > 0; chars--)
            this.uint8(0);
    };

    // Конфвертирование ArrayBuffer
    this.toArray = () =>
    {
        const result = new ArrayBuffer(buffer.length);
        const u8 = new Uint8Array(result);
        for (let i = 0; i < buffer.length; i++)
            u8[i] = buffer[i];
        return result;
    };
}

// Расширения стандартных объектов

// Удаление элемента из массива
Array.prototype.remove = function (item)
{
    const index = this.indexOf(item);
    if (index < 0)
        return false;
    
    this.splice(index, 1);
    return true;
};

// Вставка элемента по индексу
Array.prototype.insert = function (index, item)
{
    this.splice(index, 0, item);
};

// Awaitable задерка в миллисекундах
// TODO: может не нужна
function delay(mills)
{
    // Создание промиса
    let timeout;
    const result = new Promise(resolve => 
        timeout = setTimeout(resolve, mills));
    
    // Добавление функции отмены
    result.cancel = () => clearTimeout(timeout);
    
    return result;
};

// Расширения для jQuery
jQuery.fn.extend(
{
    // Показывает/скрывает/получает элемент по булвому значению
    visible(state) 
    {
        if (state === undefined)
            return this.is(":visible");
        if (state)
            this.show();
        else
            this.hide();
    },
    
    // Установка/снятие/получение состояния чекбокса
    checked(state)
    {
        if (state === undefined)
            return this.is(":checked");
        this.prop("checked", state);
        this.trigger("change");
    },

    // Установка/снятие/получение разрешения клика по кнопке
    disabled(state)
    {
        if (state === undefined)
            return this.prop("disabled");
        if (state)
            this.prop("disabled", true);
        else
            this.removeAttr("disabled");
    },
    
    // Добавление/Удаление класса
    setClass(name, state)
    {
        if (this.hasClass(name) != state)
            this.toggleClass(name);
    },
    
    // Показывает/скрывает спинер на кнопке
    spinner(state)
    {
        // Показ/Скрытие элементов
        this.children("span").css("opacity", state ? 0 : 1);
        this.children("div").visible(state);
    },
    
    // Инициализация шаблоного элемента
    template()
    {
        // Текущий идентификатор
        let id = 0;
        // Список суффиксов замены
        const suffixes = [...arguments];
        // Текстовый шаблон элемента
        const template = this.html();
        
        // Создание элемента из шаблона
        return () =>
        {
            id++;
            let html = template;
            suffixes.forEach(suffix =>
                html = html.replaceAll(suffix, suffix + "-" + id));
            return $(html);
        };
    },
    
    // Инициализация списочного элемента с шаблоном
    templateList()
    {
        // Список элементов
        const items = [];
        // Родительский контейнер
        const parent = this;
        // Текстовый шаблон элемента
        const template = this.children("template").template(...arguments);
        
        // Готовим результат
        const result = 
        {
            // Список элементов
            items: items,
            
            // Метод создания
            create()
            {
                const item = template();
                
                // Объект управления списком
                item.list =
                {
                    // Метод удаления
                    remove()
                    {
                        if (items.indexOf(item) < 0)
                            return;
                        
                        // Удаление из DOM
                        item.remove();
                        // Удаление из списка
                        items.remove(item);
                    },
                    
                    // Вставка по индексу
                    insert(index)
                    {
                        // Удаление
                        item.list.remove();
                        
                        if (index >= items.length)
                        {
                            // Добавление в DOM
                            parent.append(item);
                            items.push(item);
                            return;
                        }
                        
                        // Вставка в DOM
                        items[index].before(item);
                        items.insert(index, item);
                    },
                };
                
                return item;
            },
            
            // Метод отчистки списка
            clear()
            {
                while (items.length > 0)
                    items[0].list.remove();
            }
        };
        
        return result;
    },
});
