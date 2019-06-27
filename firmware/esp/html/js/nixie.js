
// Объект неоновой лампы
function NixieTube()
{
	// Отображаемая цифра
	this.digit = 8;
    // Вывод десятичного разделителя
    this.dot = false;
    // Насыщенность цифры
    this.sat = 255;
    // Цвет LED подсветки
    this.led = new Color(0, 0, 0, 0);
    
    // Для замыканий
    var nixie = this;
    
    // Вывод на канве
    this.draw = function (ctx)
    {
        // Размеры балона
        var rh = 106;
        var rw = 130;
        var ah = 64;
        // Толщина границы
        var bt = 16;
        // Параметры сетки
        var gs = 12;
        var gfX = 3;
        var gfY = -1;
        // Производные
        var rw2 = rw / 2;
        var ah2 = ah * 2;
        var bt2 = bt / 2;

        // Вывод цифры
        function drawDigit()
        {
            // Вывод с указанным
            function drawIternal(lineWidth, color)
            {
                // Вывод дуги через градусы
                function arc(x, y, r, as, ae, dir)
                {
                    ctx.arc(x, y, r, convert.grad2rad(as), convert.grad2rad(ae), dir);
                };
                // Верхний и нижний предел координаты цифры
                var ys = 52;
                var ye = 182;
                // Форма цифры
                ctx.beginPath();
                switch (nixie.digit)
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
            };
            drawIternal(9, new Color(0xff, 0x98, 0x1c, nixie.sat)); // #ff981c
            drawIternal(3, new Color(0xff, 0xc1, 0x77, nixie.sat)); // #ffc177
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
            var fixY = 3;
            var offset = bt2 + 1;
            buildForm(offset, offset, fixY);
            // Сетка по горизонтали
            var calcY = function (i) { return ah + gs * (i - 3) + gfY; }
            for (var i = 1; i < 15; i++)
            {
                var y = calcY(i);
                ctx.moveTo(bt2, y);
                ctx.lineTo(rw - bt2, y);
            }
            // Сетка по вертикали
            var calcX = function (i) { return i * gs + bt2 + gfX; }
            for (var i = 1; i < 9; i++)
            {
                var x = calcX(i);
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
            var pr = 6;
            var lr = 4;
            var py = bt + pr * 3 + lr;
            ctx.arc(rw2, py, pr, 0, Math.PI * 2, false);
            ctx.moveTo(rw2 + pr, rh + ah2 - py);
            ctx.arc(rw2, rh + ah2 - py, pr, 0, Math.PI * 2, false);
            ctx.lineWidth = lr * 2;
            ctx.strokeStyle = "#C78EAE";
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
            ctx.strokeStyle = ctx.shadowColor = nixie.led.toCSS();
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
            ctx.strokeStyle = nixie.led.toCSS();
            ctx.stroke();
            // Стекло
            buildForm();
            ctx.strokeStyle = "#68D1DF20";
            ctx.stroke();
            // Тиснение, блики [1]
            buildForm(-9, -9, -2);
            ctx.clip();
            buildForm(-7, -7, -2);
            ctx.strokeStyle = "#0000003a";
            ctx.lineWidth = 1;
            ctx.shadowBlur = 3;
            ctx.shadowColor = "black";
            ctx.shadowOffsetX = 2;
            ctx.shadowOffsetY = 2;
            ctx.stroke();
            // Тиснение, блики [2]
            buildForm(-10, -10, -3);
            ctx.strokeStyle = "black";
            ctx.shadowBlur = 10;
            ctx.shadowOffsetX = 1;
            ctx.shadowOffsetY = 2;
            ctx.stroke();
            // Тиснение, блики [3]
            ctx.shadowColor = "white";
            ctx.shadowBlur = 3;
            ctx.shadowOffsetX = 2;
            ctx.shadowOffsetY = 4;
            ctx.stroke();
        ctx.restore();
    };
}
