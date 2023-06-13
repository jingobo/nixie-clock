#ifndef __HMI_H
#define __HMI_H

#include "typedefs.h"
#include <list.h>

// Тип данных для индексации разряда
typedef uint8_t hmi_rank_t;

// Количество разрядов дисплея
constexpr const hmi_rank_t HMI_RANK_COUNT = 6;

// Кадров в секунду (Гц)
constexpr const uint32_t HMI_FRAME_RATE = 120;

// Тип данных для приведенного времени (0.25 сек)
typedef uint8_t hmi_time_t;

// Количество дискретов приведенного времени
constexpr const hmi_time_t HMI_TIME_COUNT = 60;

// Конвертирование приведенного времени в количество фреймов
constexpr inline uint32_t hmi_time_to_frame_count(hmi_time_t time)
{
    return HMI_FRAME_RATE / 4 * time;
}

// Количество фреймов смены состояния по умолчанию
constexpr const uint32_t HMI_SMOOTH_FRAME_COUNT = hmi_time_to_frame_count(1);

// Тип данных для хранения насыщенности компоненты
typedef uint8_t hmi_sat_t;

// Минимальная насыщенность
constexpr const hmi_sat_t HMI_SAT_MIN = 0;
// Максимальная насыщенность
constexpr const hmi_sat_t HMI_SAT_MAX = UINT8_MAX;
// Количество уровней насыщенности
constexpr const uint16_t HMI_SAT_COUNT = HMI_SAT_MAX + 1;

// Таблица уровней насыщенности
typedef hmi_sat_t hmi_sat_table_t[HMI_SAT_COUNT];

// Тип для хранения цвета в формате RGB
union hmi_rgb_t
{
    // Порядок компонент из-за специфики светодиодов WS2812B
    struct
    {
        hmi_sat_t g;
        hmi_sat_t r;
        hmi_sat_t b;
    };
    uint8_t grb[3];
     
    // Оператор равенства
    bool operator == (const hmi_rgb_t &a) const
    { 
        return r == a.r &&
               g == a.g &&
               b == a.b; 
    }

    // Оператор не равенства
    bool operator != (const hmi_rgb_t &a) const
    { 
        return r != a.r ||
               g != a.g ||
               b != a.b; 
    }

    // Применение коррекции
    void correction(const hmi_sat_table_t &table)
    {
        r = table[r];
        g = table[g];
        b = table[b];
    }
};

// Статическая инициализация
#define HMI_RGB_INIT(r, g, b)   \
    { .grb = { (g), (r), (b) } }

// Тип для хранения цвета в формате HSV
union hmi_hsv_t
{
    struct
    {
        hmi_sat_t h;
        hmi_sat_t s;
        hmi_sat_t v;
    };
    uint8_t hsv[3];
        
    // Оператор равенства
    bool operator == (const hmi_hsv_t &a) const
    {
        return h == a.h &&
               s == a.s &&
               v == a.v; 
    }

    // Оператор не равенства
    bool operator != (const hmi_hsv_t &a) const
    {
        return h != a.h ||
               s != a.s ||
               v != a.v; 
    }
        
    // Конвертирование HSV в RGB
    hmi_rgb_t to_rgb(void) const;
};

// Статическая инициализация
#define HMI_HSV_INIT(h, s, v)   \
    { .hsv = { (h), (s), (v) } }

// Класс модели фильтров
template <typename DATA, hmi_rank_t COUNT>
class hmi_model_t
{
    // Проверка индекса
    static void index_check(hmi_rank_t index)
    {
        assert(index < COUNT); 
    }
    
    // Приоритет источника
    static constexpr const uint8_t PRIORITY_SOURCE = 0;
    // Приоритет дисплея
    static constexpr const uint8_t PRIORITY_DISPLAY = UINT8_MAX;
public:
    // Тип данных
    using data_t = DATA;
    // Тип блока данных
    using data_block_t = data_t[COUNT];
    // Количество разрядов
    static constexpr const hmi_rank_t RANK_COUNT = COUNT;

    // Приоритет захвата источника
    static constexpr const uint8_t PRIORITY_CAPTURE = PRIORITY_SOURCE + 1;
    // Приоритет учёта освещенности
    static constexpr const uint8_t PRIORITY_LIGHT = PRIORITY_DISPLAY - 1;
    
    // Минимальный приоритет фильтров
    static constexpr const uint8_t PRIORITY_FILTER_MIN = PRIORITY_CAPTURE + 1;
    // Максимальный приоритет фильтров
    static constexpr const uint8_t PRIORITY_FILTER_MAX = PRIORITY_LIGHT - 1;

    // Базовый класс слоя данных
    class layer_t : protected list_item_t
    {
        friend class hmi_model_t;
        // Входящие данные
        DATA in[COUNT];
    protected:
        // Обработчик события присоединения к цепочке
        virtual void attached(void)
        { }

        // Обнвление данных
        virtual void refresh(void)
        { }

        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const = 0;
        
        // Получает приоритет слоя
        virtual uint8_t priority_get(void) const = 0;

        // Обработчик изменения стороны
        virtual void side_changed(list_side_t side) = 0;
        
        // Обработчик изменения данных
        virtual void data_changed(hmi_rank_t index, DATA &data) = 0;
        
        // Получсет данные по указанному разряду
        DATA in_get(hmi_rank_t index) const
        {
            index_check(index);
            return in[index];
        }
        
        // Ввод данных
        void input(hmi_rank_t index, DATA data)
        {
            index_check(index);
            // Изменились ли данные
            if (in[index] == data)
                return;
            
            // Обязательно скопировать
            auto next = data;
            // Обработка новых данных
            data_changed(index, next);
            // Установка новых данных
            in[index] = data;
        }
    };

    // Базовый класс трансивера данных
    class transceiver_t : public layer_t
    {
        // Исходящие данные
        DATA out[COUNT];
        
        // Получает приведенный указательн на следующий слой
        layer_t * next_layer(void) const
        {
            return (layer_t *)list_item_t::next();
        }
        
        // Вывод данных
        void output(hmi_rank_t index, DATA data) const
        {
            index_check(index);
            
            // Полуение следующего слоя
            const auto next = next_layer();
            if (next != NULL)
                next->input(index, data);
        }
        
        // Вывод данных
        void reoutput(void) const
        {
            for (hmi_rank_t i = 0; i < COUNT; i++)
                output(i, out_get(i));
        }
    protected:
        // Установка выходных данных
        void out_set(hmi_rank_t index, DATA data)
        {
            index_check(index);
            // Проверка на изменение не требуется
            out[index] = data;
            output(index, data);
        }

        // Получение выходных данных
        DATA out_get(hmi_rank_t index) const
        {
            index_check(index);
            return out[index];
        }
        
        // Обработчик события присоединения к цепочке
        virtual void attached(void)
        {
            // Базовый метод
            layer_t::attached();
            
            // Передаем данные
            reoutput();
        }
        
        // Обработчик изменения стороны
        virtual void side_changed(list_side_t side) override
        {
            switch (side)
            {
                case LIST_SIDE_PREV:
                    // Ничего не делаем
                    break;
                    
                case LIST_SIDE_NEXT:
                    // Передаем данные
                    reoutput();
                    break;
                    
                default:
                    assert(false);
            }
        }
    };
    
    // Базовый класс источника данных
    class source_t : public transceiver_t
    {
    protected:
        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const override final
        {
            // Источник всегда можно перемещать
            return true;
        }
        
        // Получает приоритет слоя
        virtual uint8_t priority_get(void) const override final
        {
            // Низший приоритет
            return PRIORITY_SOURCE;
        }
        
        // Обработчик изменения стороны
        virtual void side_changed(list_side_t side) override final
        {
            switch (side)
            {
                case LIST_SIDE_NEXT:
                    // Базовый метод
                    transceiver_t::side_changed(side);
                    break;
                    
                default:
                    assert(false);
            }
        }

        // Обработчик изменения данных TODO: возможно оставить абстрактным
        virtual void data_changed(hmi_rank_t index, DATA &data) override final
        {
            // По умолчанию нет реакции
            // В источник могут прилететь только данные предыдущей сцены
            index_check(index);
        }
    };
        
    // Базовый класс фильтра данных
    class filter_t : public transceiver_t
    {
        // Приоритет фильтра
        const uint8_t priority;
    protected:
        // Основой конструктор
        filter_t(uint8_t _priority) : priority(_priority)
        {
            assert(priority >= PRIORITY_FILTER_MIN);
            assert(priority <= PRIORITY_FILTER_MAX);
        }
    
        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const override final
        {
            // Фильтр по умолчанию можно перемещать
            return true;
        }
        
        // Получает приоритет слоя
        virtual uint8_t priority_get(void) const override final
        {
            // Указанный приоритет
            return priority;
        }

        // Обработчик изменения стороны
        virtual void side_changed(list_side_t side) override final
        {
            // Просто запечатывание метода
            transceiver_t::side_changed(side);
        }
        
        // Обработчик изменения данных
        virtual void data_changed(hmi_rank_t index, DATA &data) override
        {
            // По умолчанию передача дальше
            transceiver_t::out_set(index, data);
        }
    };
    
    // Базовый класс дисплея данных
    class display_t : public layer_t
    {
    protected:
        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const override final
        {
            // Дисплей никогда не перемещаемый
            return false;
        }
        
        // Получает приоритет слоя
        virtual uint8_t priority_get(void) const override final
        {
            // Высший приоритет
            return PRIORITY_DISPLAY;
        }
        
        // Обработчик изменения стороны
        virtual void side_changed(list_side_t side) override final
        {
            switch (side)
            {
                case LIST_SIDE_PREV:
                    // Ничего не делаем
                    break;
                    
                default:
                    assert(false);
            }
        }
    };

    // Класс контроллера плавного изменения данных
    class smoother_t
    {
        // Данные для эффекта
        struct 
        {
            // Начальные данные
            DATA from;
            // Текущий фрейм
            uint32_t frame = 0;
        } ranks[COUNT];
        
        // Общее количество фреймов
        uint32_t frame_count = 0;
    public:
        // Запуск на разряде
        void start(hmi_rank_t index, DATA from)
        {
            index_check(index);
            
            ranks[index].from = from;
            ranks[index].frame = 0;
        }
        
        // Получает необходимость обработки разряда
        bool process_needed(hmi_rank_t index) const
        {
            index_check(index);
            return ranks[index].frame < frame_count;
        }
        
        // Обработка разряда
        DATA process(hmi_rank_t index, DATA to)
        {
            index_check(index);
            assert(process_needed(index));
            
            // Инкремент фрейма
            auto& frame = ranks[index].frame;
            assert(frame < frame_count);
            frame++;

            // Обработка
            return ranks[index].from.smooth(to, frame, frame_count);
        }
        
        // Остановка на всех разрядах
        void stop(void)
        {
            for (hmi_rank_t i = 0; i < COUNT; i++)
                ranks[i].frame = frame_count;
        }
        
        // Устанвливает длительность в дискретах приведенного времени
        void time_set(hmi_time_t value)
        {
            frame_count = maximum<uint32_t>(hmi_time_to_frame_count(value), 1);
            stop();
        }
    };
private:
    // Указатель на перенаправляемую модель модель добавление/удаления фильтров
    hmi_model_t *redirect = NULL;
    // Список фильтров
    list_template_t<layer_t> list;
    
    // Оповещение сторон о смене слоёв
    static void side_changed(list_item_t *prev, list_item_t *next)
    {
        // Предыдущая
        if (prev != NULL)
            ((layer_t *)prev)->side_changed(LIST_SIDE_NEXT);
        
        // Следующая
        if (next != NULL)
            ((layer_t *)next)->side_changed(LIST_SIDE_PREV);
    }
public:
    // Обнвление данных
    void refresh(void) const
    {
        for (auto i = list.head(); i != NULL; i = LIST_ITEM_NEXT(i))
            i->refresh();
    }
    
    // Добавление слоя в цепочку (внутренний метод)
    void attach(layer_t &item, bool raise_event)
    {
        // Если уже добавлен
        if (item.linked())
        {
            assert(item.list() == &list);
            return;
        }
        
        // Поиск места вставки
        layer_t *place = NULL;
        for (auto i = list.head(); i != NULL; i = LIST_ITEM_NEXT(i))
            if (i->priority_get() > item.priority_get())
            {
                place = i;
                break;
            }
        
        // Вставка
        if (place == NULL)
            item.link(list);
        else
            item.link(place, LIST_SIDE_PREV);
        
        if (raise_event)
        {
            // Оповещение сторон о смене слоёв
            side_changed(item.prev(), item.next());
            // Генерирование события
            item.attached();
        }
    }
    
    // Добавление слоя в цепочку
    void attach(layer_t &item)
    {
        // Если есть перенаправление
        if (redirect != NULL)
            redirect->attach(item);
        else
            attach(item, true);
    }

    // Удаление слоя из цепочки (внутренний метод)
    void detach(layer_t &item, bool raise_event)
    {
        // Если уже удален
        if (item.unlinked())
            return;
        
        // Проверка, есть ли в цепочке
        assert(list.contains(item));
        
        // Получаем стороны при удалении
        const auto prev = item.prev();
        const auto next = item.unlink(LIST_SIDE_NEXT);
        
        if (raise_event)
            // Оповещение сторон о смене слоёв
            side_changed(prev, next);
    }
    
    // Удаление слоя из цепочки
    void detach(layer_t &item)
    {
        // Если есть перенаправление
        if (redirect != NULL)
            redirect->detach(item);
        else
            detach(item, true);
    }
    
    // Перенос фильтров в указанную сцену
    void move(hmi_model_t &to)
    {
        assert(redirect == NULL);
        
        // Состояния направления переноса
        const bool forward = to.redirect == NULL;
        const bool backward = to.redirect != NULL;
        
        // Если возвращаем фильтры
        if (backward)
        {
            // Сброс переноса
            assert(to.redirect == this);
            to.redirect = NULL;
        }
        else
            redirect = &to;
        
        // Последние выводиме данные дисплея
        DATA data[COUNT];
        {
            const auto last = to.list.last();
            if (last != NULL)
                for (hmi_rank_t i = 0; i < COUNT; i++)
                    data[i] = last->in_get(i);
        }
        
        // Перенос слоёв
        for (auto i = list.head(); i != NULL;)
        {
            // Получаем ссылку на слой
            auto &layer = *i;
            // Переход к следующему слою
            i = LIST_ITEM_NEXT(i);
            
            // Проверяем, можно ли перенести
            if (!layer.moveable_get())
                continue;
            
            // Удаление
            detach(layer, backward);
            // Добавление
            to.attach(layer, forward);
        }
        
        // Применение последних данных
        {
            const auto head = to.list.head();
            if (head != NULL)
                for (hmi_rank_t i = 0; i < COUNT; i++)
                    head->input(i, data[i]);
        }
    }
};

// Таблица коррекции гаммы (g = 2.2)
extern const hmi_sat_table_t HMI_GAMMA_TABLE;

// Цвета RGB
constexpr const hmi_rgb_t 
    HMI_COLOR_RGB_BLACK = HMI_RGB_INIT(HMI_SAT_MIN, HMI_SAT_MIN, HMI_SAT_MIN),
    HMI_COLOR_RGB_RED = HMI_RGB_INIT(HMI_SAT_MAX, HMI_SAT_MIN, HMI_SAT_MIN),
    HMI_COLOR_RGB_GREEN = HMI_RGB_INIT(HMI_SAT_MIN, HMI_SAT_MAX, HMI_SAT_MIN),
    HMI_COLOR_RGB_BLUE = HMI_RGB_INIT(HMI_SAT_MIN, HMI_SAT_MIN, HMI_SAT_MAX),
    HMI_COLOR_RGB_WHITE = HMI_RGB_INIT(HMI_SAT_MAX, HMI_SAT_MAX, HMI_SAT_MAX);

#endif // __HMI_H
