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

// Назначение передатчика данных
enum hmi_filter_purpose_t
{
    // Источник данных
    HMI_FILTER_PURPOSE_SOURCE,
    
    // Плавная коррекция значения
    HMI_FILTER_PURPOSE_SMOOTH_VAL,
    // Плавная коррекция насыщенности
    HMI_FILTER_PURPOSE_SMOOTH_SAT,
    
    // Мгновенная коррекция значения
    HMI_FILTER_PURPOSE_INSTANT_VAL,
    // Мгновенная коррекция насыщенности
    HMI_FILTER_PURPOSE_INSTANT_SAT,
    
    // ---  Не перемещаемые --- //
    
    // Конечный дисплей (приёмнк данных)
    HMI_FILTER_PURPOSE_DISPLAY
};

// Класс модели фильтров
template <typename DATA, hmi_rank_t COUNT>
class hmi_model_t
{
public:
    // Базовый класс фильтра 
    class filter_t : list_item_t
    {
        friend class hmi_model_t;
        // Последние принятые данные
        DATA last[COUNT];
        // Назначение фильтра
        const hmi_filter_purpose_t purpose;
        
        // Проверка индекса
        static void index_check(hmi_rank_t index)
        { 
            assert(index < COUNT); 
        }
        
        // Передача данный следующему фильтру
        void resend(void)
        {
            for (hmi_rank_t i = 0; i < COUNT; i++)
            {
                // Обязательное копирование и обработка
                auto tmp = last[i];
                process(i, tmp);
            }
        }
        
        // Получает, можно ли фильтр переносить в другую модель
        bool moveable(void) const
        {
            return purpose != HMI_FILTER_PURPOSE_DISPLAY;
        }
        
        // Устанавливает новые дныые следующему фильтру
        void data_set_next(hmi_rank_t index, DATA data) const
        {
            index_check(index);
            // Полуение следующего фильтра
            auto next = next_cast();
            if (next != NULL)
                next->data_set(index, data);
        }
    protected:
        // Основой конструктор
        filter_t(hmi_filter_purpose_t _purpose) : purpose(_purpose)
        { }
        
        // Получает приведенный указательн на следующий фильтр
        filter_t * next_cast(void) const
        {
            return (filter_t *)next();
        }

        // Событие присоединения к цепочке
        virtual void attached(void)
        { }
        
        // Обнвление данных
        virtual void refresh(void)
        { }
        
        // Событие обработки новых данных
        virtual void process(hmi_rank_t index, DATA &data)
        {
            index_check(index);
            // Переход к следующему фильтру
            data_set_next(index, data);
        }

        // Получает текущие данные
        DATA data_get(hmi_rank_t index) const
        { 
            index_check(index);
            return last[index]; 
        }
        
        // Устанавливает новые дныые
        void data_set(hmi_rank_t index, DATA data)
        {
            index_check(index);
            // Изменились ли данные
            if (last[index] == data)
                return;
            // Обязательно скопировать
            auto next = data;
            // Обработка новых данных
            process(index, next);
            // Установка новых данных
            last[index] = data;
        }
                        
        // Смена данных на разряде (относительно назначения фильтра)
        void data_change(hmi_rank_t index, DATA data)
        {
            switch (purpose)
            {
                case HMI_FILTER_PURPOSE_SOURCE:
                    // Устанавливаем себе
                    data_set(index, data);
                    break;
                case HMI_FILTER_PURPOSE_DISPLAY:
                    // Ничего не далем
                    break;
                default:
                    // По умолчанию передаем дальше
                    data_set_next(index, data);
                    break;
            }
        }
    };
private:
    // Указатель на перенаправляемую модель модель добавление/удаления фильтров
    hmi_model_t *redirect;
    // Список фильтров
    list_template_t<filter_t> list;
    
    // Передача данный следующему фильтру
    static void resend(list_item_t *filter)
    {
        if (filter == NULL)
            return;
        ((filter_t *)filter)->resend();
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
    
    // Добавление фильтра в цепочку
    void attach(filter_t &item)
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
        filter_t *place = NULL;
        for (auto i = list.head(); i != NULL; i = LIST_ITEM_NEXT(i))
            if (i->purpose > item.purpose)
            {
                place = i;
                break;
            }
        // Вставка
        if (place == NULL)
            item.link(list);
        else
            item.link(place, LIST_SIDE_PREV);
        // Обновление данных
        resend(item.prev());
        // Генерирование события
        item.attached();
    }
    
    // Удаление фильтра из цепочки
    void detach(filter_t &item)
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
        // Расцепление
        auto prev = item.unlink(LIST_SIDE_PREV);
        // Обновление данных
        resend(prev);
    }
    
    // Перенос фильтров кроме диспеля и гаммы в указанную сцену
    void move(hmi_model_t &to)
    {
        assert(redirect == NULL);
        // Если возвращаем фильтры
        auto backward = to.redirect != NULL;
        if (backward)
        {
            assert(to.redirect == this);
            to.redirect = NULL;
        }
        // Если уже перенесены
        assert(redirect == NULL);
        // Последние выводиме данные дисплея
        DATA last_displayed[COUNT];
        // Не перемещаемые фильтры переносим во временный список
        list_template_t<filter_t> non_moveable;
        for (auto i = to.list.head(); i != NULL;)
        {
            // Получаем ссылку на фильтр
            auto &filter = *i;
            // Переход к следующему фильтру
            i = LIST_ITEM_NEXT(i);
            // Проверка возможности переноса
            assert(!filter.moveable());
            // Если первый не перемещаемый фильтр
            if (non_moveable.empty())
                // Сохранение его данные
                for (hmi_rank_t j = 0; j < COUNT; j++)
                    last_displayed[j] = filter.data_get(j);
            // Перенос во временный список
            filter.unlink();
            filter.link(non_moveable);
        }
        // Перенос с начала
        for (auto i = list.head(); i != NULL;)
        {
            // Получаем ссылку на фильтр
            auto &filter = *i;
            // Переход к следующему фильтру
            i = LIST_ITEM_NEXT(i);
            // Проверяем, можно ли перенести
            if (filter.moveable())
            {
                // Удаление
                detach(filter);
                // Если первый фильтр - применение последних данных
                if (to.list.empty())
                    for (hmi_rank_t j = 0; j < COUNT; j++)
                        filter.data_set(j, last_displayed[j]);
                // Добавление
                to.attach(filter);
            }
        }
        // Возвращаем не перемещаемые объекты   
        for (auto i = non_moveable.head(); i != NULL;)
        {
            // Получаем ссылку на фильтр
            auto &filter = *i;
            // Переход к следующему фильтру
            i = LIST_ITEM_NEXT(i);
            // Возвращение в исходный список
            filter.unlink();
            // Не перемещаемые объекты всегда с конца
            filter.link(to.list);
            // Получение данных
            resend(filter.prev());
        }
        // Указываем куда перенесены
        if (!backward)
            redirect = &to;
    }
};

// Таблица коррекции гаммы (g = 2.2)
extern const hmi_sat_table_t HMI_GAMMA_TABLE;

// Цвета HSV
extern const hmi_hsv_t 
    HMI_COLOR_HSV_BLACK,
    HMI_COLOR_HSV_RED,
    HMI_COLOR_HSV_GREEN,
    HMI_COLOR_HSV_BLUE;

// Цвета RGB
extern const hmi_rgb_t 
    HMI_COLOR_RGB_BLACK,
    HMI_COLOR_RGB_RED,
    HMI_COLOR_RGB_GREEN,
    HMI_COLOR_RGB_BLUE,
    HMI_COLOR_RGB_WHITE;

// Функция приведения чиисла from в to за frame_cout фреймов
template <typename T>
static inline T hmi_drift(T from, T to, uint32_t frame, uint32_t frame_cout)
{
    assert(frame_cout > 0);
    // Определяем разницу
    auto dx = (int32_t)to - from;
    // Расчет
    if (dx >= 0)
        return (T)(from + (uint32_t)dx * frame / frame_cout);
    dx = -dx;
    return (T)(from - (uint32_t)dx * frame / frame_cout);
}

#endif // __HMI_H
