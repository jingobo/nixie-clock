#ifndef __SCREEN_H
#define __SCREEN_H

#include "led.h"
#include "neon.h"
#include "nixie.h"

// Класс модели экрана
class screen_model_t
{
public:
    led_model_t led;
    neon_model_t neon;
    nixie_model_t nixie;
protected:
    // Обновление сцены
    virtual void refresh(void)
    { }

    // Секундное событие
    virtual void second(void)
    { }
    
    // Перенос фильтров в указанную сцену
    void move(screen_model_t &to)
    {
        led.move(to.led);
        neon.move(to.neon);
        nixie.move(to.nixie);
    }
};

// Базовый класс сцены
class screen_scene_t : public screen_model_t
{
    friend class screen_t;
protected:
    // Событие активации сцены на дисплее
    virtual void activated(void)
    { }
    
    // Событие деактивации сцены на дисплее
    virtual void deactivated(void)
    { }
};

// Класс экрана
class screen_t : public screen_model_t
{
    friend void screen_second_event_cb(void);
    // Указатель на текущую сцену
    screen_scene_t *scene = NULL;
public:
    // Получает текущую отображаемую схему
    screen_scene_t * scene_get(void) const
    {
        return scene;
    }
    
    // Устпновка текущеей сцены
    void scene_set(screen_scene_t *ref)
    {
        if (ref == scene_get())
            return;
        
        // Отсоединение старой сцены
        if (scene != NULL)
        {
            move(*scene);
            scene->deactivated();
        }

        scene = ref;
        
        // Присоединение новой
        if (scene != NULL)
        {
            scene->activated();
            scene->move(*this);
        }
    }

    // Устпновка текущеей сцены
    void scene_set(screen_scene_t &ref)
    {
        scene_set(&ref);
    }
    
    // Обновление сцены (повышение доступа)
    virtual void refresh(void) override final
    {
        // Базовый метод
        screen_model_t::refresh();
        
        // Обновление моделей
        led.refresh();
        neon.refresh();
        nixie.refresh();
        
        // Вызов обнвоения у сцены
        if (scene != NULL)
            scene->refresh();
    }
    
    // Секундное событие (повышение доступа)
    virtual void second(void) override final
    {
        // Базовый метод
        screen_model_t::second();
        // Оповещение сцены
        if (scene != NULL)
            scene->second();
    }
};

// Экран
extern screen_t screen;

// Инициализация модуля
void screen_init(void);

#endif // __SCREEN_H
