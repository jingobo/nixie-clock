
// Общее у ламп
const tubeCommon =
{
    // Цвет стекла
    glassColorCSS: "#68D1DF20",
    // Цвет границы
    borderColorCSS: "#0000003a",
    // Цвет тени
    shadowColorCSS: "black",
    // Цвет освещения
    lightColorCSS: "white",
    
    // Получает основной цвет свечения
    getPrimaryColor(sat)
    {
        return new Color(0xff, 0x98, 0x1c, sat);
    },
    
    // Получает вторичный цвет свечения
    getSecondaryColor(sat)
    {
        return new Color(0xff, 0xc1, 0x77, sat);
    },
};

// Объект неоновой лампы
function NeonTube()
{
    // Насыщенность точки
    this.sat = 255;

    // Вывод на канве
    this.draw = ctx =>
    {
        // Диаметр
        const d = 24;
        // Радиус
        const r = d / 2;
        
        // Создание формы балона
        function buildForm(offset)
        {
            // Аргументы по умолчанию
            if (offset === undefined)
                offset = 0;
            
            // Начало вывода
            ctx.beginPath();
            // Окружность
            ctx.arc(r / 2, r / 2, r + offset, 0, Math.PI * 2);
        };
        
        // Вывод свечения
        function drawLight(offset, color)
        {
            buildForm(offset);
            
            // Параметры вывода
            ctx.fillStyle = ctx.shadowColor = color.toCSS();
            ctx.shadowBlur = 6 - offset;
            // Вывод
            ctx.fill();
        }
        
        ctx.save();
            buildForm();
            
            // Свечение
            drawLight(0, tubeCommon.getPrimaryColor(this.sat));
            drawLight(-1, tubeCommon.getSecondaryColor(this.sat));
            ctx.shadowBlur = 0;
            
            // Стекло
            buildForm();
            ctx.fillStyle = tubeCommon.glassColorCSS;
            ctx.fill();
            
            // Тиснение, блики [1]    
            buildForm(1);
            ctx.clip();
            buildForm();
            ctx.strokeStyle = tubeCommon.borderColorCSS;
            ctx.lineWidth = 1;
            ctx.shadowBlur = 3;
            ctx.shadowColor = tubeCommon.shadowColorCSS;
            ctx.shadowOffsetX = 2;
            ctx.shadowOffsetY = 2;
            ctx.stroke();
            
            // Тиснение, блики [2]
            buildForm(3);
            ctx.strokeStyle = tubeCommon.shadowColorCSS;
            ctx.shadowBlur = 10;
            ctx.shadowOffsetX = 1;
            ctx.shadowOffsetY = 2;
            ctx.stroke();
            
            // Тиснение, блики [3]
            ctx.shadowColor = tubeCommon.lightColorCSS;
            ctx.shadowBlur = 3;
            ctx.shadowOffsetX = 2;
            ctx.shadowOffsetY = 4;
            ctx.stroke();
        
        ctx.restore();
    };
}


// Объект основной лампы
function NixieTube()
{
    // Отображаемая цифра
    this.digit = 0;
    // Вывод десятичного разделителя
    this.dot = false;
    // Насыщенность цифры
    this.sat = 255;
    // Цвет LED подсветки
    this.led = new Color(0, 0, 0, 0);
        
    // Вывод на канве
    this.draw = function (ctx)
    {
        // Размеры балона
        const rh = 106;
        const rw = 130;
        const ah = 64;
        // Толщина границы
        const bt = 16;
        // Параметры сетки
        const gs = 12;
        const gfX = 3;
        const gfY = -1;
        // Производные
        const rw2 = rw / 2;
        const ah2 = ah * 2;
        const bt2 = bt / 2;

        // Для замыканий
        const self = this;
        
        // Вывод цифры
        function drawDigit()
        {
            // Вывод с указанным
            function drawIternal(lineWidth, color)
            {
                // Вывод дуги через градусы
                function arc(x, y, r, as, ae, dir)
                {
                    // Перевод градусов в радианы
                    var grad2rad = angle => angle * Math.PI / 180;
                    
                    ctx.arc(x, y, r, grad2rad(as), grad2rad(ae), dir);
                };
                
                // Верхний и нижний предел координаты цифры
                const ys = 52;
                const ye = 182;
                
                // Форма цифры
                ctx.beginPath();
                switch (self.digit)
                {
                    case 0:
                        ctx.moveTo(rw2 - 34, ys + 70);
                        ctx.bezierCurveTo(rw2 - 34, ys - 17, 
                                          rw2 + 34, ys - 17,
                                          rw2 + 34, ys + 70);

                        ctx.bezierCurveTo(rw2 + 34, ye + 16, 
                                          rw2 - 34, ye + 16,
                                          rw2 - 34, ye - 70);
                        break;
                    case 1:
                        ctx.moveTo(rw2, ys);
                        ctx.lineTo(rw2, ye);
                        break;
                    case 2:
                        ctx.moveTo(rw2 + 38, ye - 4);
                        ctx.lineTo(rw2 - 32, ye - 4);
                        arc(rw2 + 2, ye - 17 - 4, 34, 180, -90, false);
                        arc(rw2, ye - 34 - 21 - 35, 35, 90, -155, true);
                        break;
                    case 3:
                        ctx.moveTo(rw2 - 37, ys + 4);
                        ctx.lineTo(rw2 + 30, ys + 4);
                        ctx.lineTo(rw2 + 4, ys + 58);
                        arc(rw2 - 2, ys + 58 + 35, 35, -75, 155, false);
                        break;
                    case 4:
                        ctx.moveTo(rw2 + 38, ye - 40);
                        ctx.lineTo(rw2 - 30, ye - 40);
                        ctx.lineTo(rw2 + 20, ys + 12);
                        ctx.lineTo(rw2 + 20, ye);
                        break;
                    case 5:
                        ctx.moveTo(rw2 + 36, ys + 4);
                        ctx.lineTo(rw2 - 30, ys + 4);
                        arc(rw2 + 4, ys + 17 + 4, 34, -180, -270, true);
                        arc(rw2, ys + 34 + 21 + 36, 36, -90, 155, false);
                        break;
                    case 6:
                        ctx.moveTo(rw2, ys + 2);
                        ctx.lineTo(rw2 - 30, ys + 80);
                        arc(rw2, ys + 93, 33, -180, -179, true);
                        break;
                    case 7:
                        ctx.moveTo(rw2 - 38, ys + 4);
                        ctx.lineTo(rw2 + 30, ys + 4);
                        ctx.lineTo(rw2, ye - 2);
                        break;
                    case 8:
                        arc(rw2, ys + 29, 26, 0, 360, false);
                        ctx.moveTo(rw2 + 34, 142);
                        arc(rw2, ye - 39, 35, 0, 360, false);
                        break;
                    case 9:
                        ctx.moveTo(rw2, ye - 2);
                        ctx.lineTo(rw2 + 30, ye - 80);
                        arc(rw2, ye - 93, 33, -1, 0, true);
                        break;
                }
                
                // Параметры вывода
                ctx.lineWidth = lineWidth;
                ctx.shadowBlur = lineWidth * 2;
                ctx.strokeStyle = ctx.shadowColor = color.toCSS();
                // Вывод
                ctx.stroke();

                // Десятичная точка
                if (!self.dot)
                    return;
                
                // Форма точки
                ctx.beginPath();
                arc(rw2 - 47, ye - 5, 3, 0, 360, true);

                // Вывод
                ctx.stroke();
            };
            drawIternal(9, tubeCommon.getPrimaryColor(self.sat));
            drawIternal(3, tubeCommon.getSecondaryColor(self.sat));
            ctx.shadowBlur = 0;
        };

        // Создание формы балона
        function buildForm(offsetX, offsetY, fixY)
        {
            // Аргументы по умолчанию
            if (offsetX === undefined)
                offsetX = 0;
            if (offsetY === undefined)
                offsetY = 0;
            if (fixY === undefined)
                fixY = 0;
            
            // Начало вывода
            ctx.beginPath();
            // Верхняя арка
            ctx.moveTo(offsetX, ah);
            ctx.bezierCurveTo(offsetX, offsetY + fixY, 
                              rw - offsetX, offsetY + fixY,
                              rw - offsetX, ah);
            
            // Правая сторона
            ctx.lineTo(rw - offsetX, ah + rh);
            
            // Нижняя арка
            ctx.moveTo(rw - offsetX, ah + rh);
            ctx.bezierCurveTo(rw - offsetX, ah2 + rh - offsetY - fixY, 
                              offsetX, ah2 + rh - offsetY - fixY, 
                              offsetX, ah + rh);
            
            // Левая линия
            ctx.lineTo(offsetX, ah);
        };

        // Вывод сетки
        function drawGrid()
        {
            // Граница
            const fixY = 3;
            {
                const offset = bt2 + 1;
                buildForm(offset, offset, fixY);
            }
            
            // Сетка по горизонтали
            const calcY = i => ah + gs * (i - 3) + gfY;
            for (var i = 1; i < 15; i++)
            {
                const y = calcY(i);
                ctx.moveTo(bt2, y);
                ctx.lineTo(rw - bt2, y);
            }
            
            // Сетка по вертикали
            const calcX = i => i * gs + bt2 + gfX;
            for (var i = 1; i < 9; i++)
            {
                const x = calcX(i);
                ctx.moveTo(x, bt * 2);
                ctx.lineTo(x, rh + ah2 - bt * 2);
            }
            
            // Вывод линий
            ctx.lineWidth = 1;
            ctx.strokeStyle = "gray";
            ctx.stroke();
            
            // Залитые частички
            ctx.fillStyle = ctx.strokeStyle;
            ctx.beginPath();
            ctx.moveTo(calcX(1), calcY(0) - fixY);
            ctx.lineTo(calcX(8), calcY(0) - fixY);
            ctx.lineTo(calcX(8), calcY(1));
            ctx.lineTo(calcX(1), calcY(1));
            ctx.moveTo(calcX(1), calcY(14));
            ctx.lineTo(calcX(8), calcY(14));
            ctx.lineTo(calcX(8), calcY(15) + fixY);
            ctx.lineTo(calcX(1), calcY(15) + fixY);
            ctx.fill();
            
            // Держатели цифр
            ctx.beginPath();
            const pr = 6;
            const lr = 4;
            const py = bt + pr * 3 + lr;
            ctx.arc(rw2, py, pr, 0, Math.PI * 2, false);
            ctx.moveTo(rw2 + pr, rh + ah2 - py);
            ctx.arc(rw2, rh + ah2 - py, pr, 0, Math.PI * 2, false);
            ctx.lineWidth = lr * 2;
            ctx.strokeStyle = "#d2acc2";
            ctx.fill();
            ctx.stroke();
        };
        
        // Подготовка
        ctx.save();
            ctx.shadowOffsetX = 0;
            ctx.shadowOffsetY = 0;
            
            // Тень LED подсветка
            buildForm();
            ctx.lineWidth = bt;
            ctx.strokeStyle = ctx.shadowColor = this.led.toCSS();
            ctx.shadowBlur = 15;
            ctx.stroke();
            ctx.shadowBlur = 0;
            
            // Фон
            buildForm();
            ctx.fillStyle = "#686868";
            ctx.fill();
            
            // Вывод цифры
            drawDigit();
            
            // Вывод сетки
            drawGrid();
            
            // Белая рамка
            buildForm();
            ctx.lineWidth = bt;
            ctx.strokeStyle = "white";
            ctx.stroke();
            
            // LED
            buildForm();
            ctx.strokeStyle = this.led.toCSS();
            ctx.stroke();
            
            // Стекло
            buildForm();
            ctx.strokeStyle = tubeCommon.glassColorCSS;
            ctx.stroke();
            
            // Тиснение, блики [1]
            buildForm(-9, -9, -2);
            ctx.clip();
            buildForm(-7, -7, -2);
            ctx.strokeStyle = tubeCommon.borderColorCSS;
            ctx.lineWidth = 1;
            ctx.shadowBlur = 3;
            ctx.shadowColor = tubeCommon.shadowColorCSS;
            ctx.shadowOffsetX = 2;
            ctx.shadowOffsetY = 2;
            ctx.stroke();
            
            // Тиснение, блики [2]
            buildForm(-10, -10, -3);
            ctx.strokeStyle = tubeCommon.shadowColorCSS;
            ctx.shadowBlur = 10;
            ctx.shadowOffsetX = 1;
            ctx.shadowOffsetY = 2;
            ctx.stroke();
            
            // Тиснение, блики [3]
            ctx.shadowColor = tubeCommon.lightColorCSS;
            ctx.shadowBlur = 3;
            ctx.shadowOffsetX = 2;
            ctx.shadowOffsetY = 4;
            ctx.stroke();
        ctx.restore();
    };
}
