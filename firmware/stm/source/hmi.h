#ifndef __HMI_H
#define __HMI_H

#include "typedefs.h"
#include <list.h>

// Количество разрядов дисплея
#define HMI_RANK_COUNT          6
// Кадров в секунду (Гц)
#define HMI_FRAME_RATE          120
// Кадров в секунду (Гц) / 2
#define HMI_FRAME_RATE_HALF     (HMI_FRAME_RATE / 2)

// Минимальная насыщенность
#define HMI_SAT_MIN             0
// Максимальная насыщенность
#define HMI_SAT_MAX             UINT8_MAX
// Количество уровней насыщенности
#define HMI_SAT_COUNT           (HMI_SAT_MAX + 1U)

// Тип данных для индексации разряда
typedef uint8_t hmi_rank_t;
// Тип данных для хранения насыщенности компоненты
typedef uint8_t hmi_sat_t;

// Таблица уровней насыщенности
typedef hmi_sat_t hmi_sat_table_t[HMI_SAT_COUNT];

// Тип для хранения цвета в формате RGB
union hmi_rgb_t
{
    uint32_t raw;
    // Порядок компонент из-за специфики светодиодов WS2812B
    struct
    {
        hmi_sat_t g;
        hmi_sat_t r;
        hmi_sat_t b;
        // Для прозрачности (не используется)
        hmi_sat_t a;
    };
    uint8_t grba[4];
    
    // Конструктор по умолчанеию
    hmi_rgb_t(void) : raw(0)
    { }
    
    // Конструктор с указанием компонент
    hmi_rgb_t(hmi_sat_t _r, hmi_sat_t _g, hmi_sat_t _b) : r(_r), g(_g), b(_b), a(0) 
    { }

    // Оператор равенства
    bool operator == (const hmi_rgb_t& a) const
    { 
        return raw == a.raw; 
    }

    // Оператор не равенства
    bool operator != (const hmi_rgb_t& a) const
    { 
        return raw != a.raw; 
    }

    // Оператор присваивания
    hmi_rgb_t & operator = (const hmi_rgb_t& source)
    {
        raw = source.raw;
        return *this;
    }

    // Применение коррекции
    void correction(const hmi_sat_table_t &table)
    {
        r = table[r];
        g = table[g];
        b = table[b];
    }
};

// Тип для хранения цвета в формате HSV
union hmi_hsv_t
{
    uint32_t raw;
    struct
    {
        hmi_sat_t h;
        hmi_sat_t s;
        hmi_sat_t v;
        // Для прозрачности (не используется)
        hmi_sat_t a;
    };
    uint8_t hsva[4];
    
    // Конструктор по умолчанеию
    hmi_hsv_t(void) : raw(0)
    { }

    // Конструктор с указанием компонент
    hmi_hsv_t(hmi_sat_t _h, hmi_sat_t _s = HMI_SAT_MAX, hmi_sat_t _v = HMI_SAT_MAX) 
        : h(_h), s(_s), v(_v), a(0) 
    { }
    
    // Оператор равенства
    bool operator == (const hmi_hsv_t& a) const
    { 
        return raw == a.raw;
    }

    // Оператор не равенства
    bool operator != (const hmi_hsv_t& a) const
    { 
        return raw != a.raw;
    }
    
    // Оператор присваивания
    hmi_hsv_t & operator = (const hmi_hsv_t& source)
    {
        raw = source.raw;
        return *this;
    }
    
    // Конвертирование HSV в RGB
    hmi_rgb_t to_rgb(void) const;
};

// Класс контроллера фреймов
class hmi_frame_controller_t
{
    // Хранит номер фрейма
    uint32_t frame = 0;
public:
    // Сброс фреймов
    void reset(void)
    {
        frame = 0;
    }
    
    // Оператор постинкремента
    uint32_t operator ++ (int dx)
    {
        assert(dx == 0);
        return ++frame;
    }

    // Оператор преинкремента
    uint32_t operator ++ (void)
    {
        return ++frame;
    }
    
    // Получает количество секунд
    uint32_t seconds_get(void) const
    {
        return frame / HMI_FRAME_RATE;
    }
    
    // Получает текущее значение периода секунды
    uint32_t seconds_period_get(void) const
    {
        return frame % HMI_FRAME_RATE;
    }
    
    // Получает количество полусекунд
    uint32_t half_seconds_get(void) const
    {
        return frame / HMI_FRAME_RATE_HALF;
    }
    
    // Получает текущее значение периода полусекунды
    uint32_t half_seconds_period_get(void) const
    {
        return frame % HMI_FRAME_RATE_HALF;
    }
};

// Класс модели фильтров
template <typename DATA, hmi_rank_t COUNT>
class hmi_model_t
{
    // Проверка индекса
    static void index_check(hmi_rank_t index)
    {
        assert(index < COUNT); 
    }
    
    // Статические приортитеты
    enum
    {
        // Приоритет источника
        LAYER_PRIORITY_SOURCE = INT8_MIN,
        // Приоритет перехватчика источника
        LAYER_PRIORITY_SOURCE_HOOK,
        // Приоритет дисплея
        LAYER_PRIORITY_DISPLAY = INT8_MAX,
    };
public:
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
        virtual int8_t priority_get(void) const = 0;

        // Обработчик изменения стороны
        virtual void side_changed(list_side_t side) = 0;
        
        // Обработчик изменения данных
        virtual void data_changed(hmi_rank_t index, DATA &data) = 0;
        
        // Получсет данные по указанному разряду
        DATA get(hmi_rank_t index) const
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
        
    protected:
        // Вывод данных
        virtual void output(hmi_rank_t index, DATA &data)
        {
            index_check(index);
            
            // Полуение следующего слоя
            auto next = next_layer();
            if (next != NULL)
                next->input(index, data);
        }
        
    public:
        // Вывод данных
        void reoutput(void)
        {
            for (hmi_rank_t i = 0; i < COUNT; i++)
            {
                auto data = out[i];
                output(i, data);
            }
        }
        
    protected:
        // Установка данных
        void set(hmi_rank_t index, DATA data)
        {
            out[index] = data;
            output(index, data);
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
        virtual int8_t priority_get(void) const override final
        {
            // Низший приоритет
            return LAYER_PRIORITY_SOURCE;
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

        // Обработчик изменения данных
        virtual void data_changed(hmi_rank_t index, DATA &data) override
        {
            // По умолчанию нет реакции
            // В источник могут прилететь только данные предыдущей сцены
            index_check(index);
        }
        
        // Вывод данных
        virtual void output(hmi_rank_t index, DATA &data) override final
        {
            // Просто запечатывание метода
            transceiver_t::output(index, data);
        }
    };
    
    // Базовый класс перехватчика источника данных
    class source_hook_t : public transceiver_t
    {
    protected:
        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const override final
        {
            // Перехватчика источника перемещать нельзя
            return false;
        }
        
        // Получает приоритет слоя
        virtual int8_t priority_get(void) const override final
        {
            // Низший приоритет
            return LAYER_PRIORITY_SOURCE_HOOK;
        }
        
        // Обработчик изменения стороны
        virtual void side_changed(list_side_t side) override final
        {
            // Просто запечатывание метода
            transceiver_t::side_changed(side);
        }
        
        // Обработчик изменения данных
        virtual void data_changed(hmi_rank_t index, DATA &data) override final
        {
            data_store(index, data);
            // По умолчанию передача дальше
            transceiver_t::set(index, data);
        }
        
        // Вывод данных
        virtual void output(hmi_rank_t index, DATA &data) override final
        {
            // Просто запечатывание метода
            transceiver_t::output(index, data);
        }
        
        // Оповещение о сохранении данных
        virtual void data_store(hmi_rank_t index, const DATA &data) = 0;
    };
    
    // Базовый класс фильтра данных
    class filter_t : public transceiver_t
    {
        // Приоритет фильтра
        const int8_t priority;
    protected:
        // Основой конструктор
        filter_t(int8_t _priority) : priority(_priority)
        {
            assert(priority > LAYER_PRIORITY_SOURCE_HOOK);
            assert(priority < LAYER_PRIORITY_DISPLAY);
        }
    
        // Получает, можно ли слой переносить в другую модель
        virtual bool moveable_get(void) const override
        {
            // Фильтр по умолчанию можно перемещать
            return true;
        }
        
        // Получает приоритет слоя
        virtual int8_t priority_get(void) const override final
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
            transceiver_t::set(index, data);
        }
        
        // Вывод данных
        virtual void output(hmi_rank_t index, DATA &data) override final
        {
            // Трансформация
            transform(index, data);
            // Базовый метод
            transceiver_t::output(index, data);
        }
        
        // Трансформация данных
        virtual void transform(hmi_rank_t index, DATA &data) const
        {
            // По умолчанию не реализованно (только для пассивных фильтров)
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
        virtual int8_t priority_get(void) const override final
        {
            // Высший приоритет
            return LAYER_PRIORITY_DISPLAY;
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
private:
    // Указатель на перенаправляемую модель модель добавление/удаления фильтров
    hmi_model_t *redirect;
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
    // Конструктор по умолчанию
    hmi_model_t(void) : redirect(NULL)
    { }

    // Обнвление данных
    void refresh(void) const
    {
        for (auto i = list.head(); i != NULL; i = LIST_ITEM_NEXT(i))
            i->refresh();
    }
    
    // Добавление слоя в цепочку
    void attach(layer_t &item)
    {
        // Если уже добавлен
        if (item.linked())
        {
            assert(item.list() == &list);
            return;
        }
        
        // Если есть перенаправление
        if (redirect != NULL)
        {
            redirect->attach(item);
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
        
        // Оповещение сторон о смене слоёв
        side_changed(item.prev(), item.next());
        // Генерирование события
        item.attached();
    }
    
    // Удаление слоя из цепочки
    void detach(layer_t &item)
    {
        // Если уже удален
        if (item.unlinked())
            return;
        
        // Если есть перенаправление
        if (redirect != NULL)
        {
            redirect->detach(item);
            return;
        }
        
        // Проверка, есть ли в цепочке
        assert(list.contains(item));
        
        // Получаем стороны при удалении
        auto prev = item.prev();
        auto next = item.unlink(LIST_SIDE_NEXT);
        
        // Оповещение сторон о смене слоёв
        side_changed(prev, next);
    }
    
    // Перенос фильтров в указанную сцену
    void move(hmi_model_t &to)
    {
        assert(redirect == NULL);
        
        // Если возвращаем фильтры
        const auto backward = to.redirect != NULL;
        if (backward)
        {
            // Сброс переноса
            assert(to.redirect == this);
            to.redirect = NULL;
        }
        
        // К этому моменту должен быть сброшен
        assert(redirect == NULL);
        
        // Последние выводиме данные дисплея
        DATA data[COUNT];
        {
            auto last = to.list.last();
            if (last != NULL)
                for (hmi_rank_t i = 0; i < COUNT; i++)
                    data[i] = last->get(i);
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
            detach(layer);
            // Добавление
            to.attach(layer);
        }

        // Указываем куда перенесены
        if (!backward)
            redirect = &to;
        
        // Применение последних данных
        {
            auto head = to.list.head();
            if (head != NULL)
                for (hmi_rank_t i = 0; i < COUNT; i++)
                    head->input(i, data[i]);
        }
    }
};

// Таблица коррекции гаммы (g = 2.2)
extern const hmi_sat_table_t HMI_GAMMA_TABLE;

// Цвета HSV
extern const hmi_hsv_t 
    HMI_COLOR_HSV_BLACK,
    HMI_COLOR_HSV_RED,
    HMI_COLOR_HSV_GREEN,
    HMI_COLOR_HSV_BLUE,
    HMI_COLOR_HSV_WHITE;

// Цвета RGB
extern const hmi_rgb_t 
    HMI_COLOR_RGB_BLACK,
    HMI_COLOR_RGB_RED,
    HMI_COLOR_RGB_GREEN,
    HMI_COLOR_RGB_BLUE,
    HMI_COLOR_RGB_WHITE;

#endif // __HMI_H
