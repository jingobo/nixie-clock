#include "mcu.h"
#include "clk.h"
#include "nvic.h"
#include "event.h"
#include "system.h"
#include "storage.h"

// Секция инициализации хранилища (во flash)
#define STORAGE_SECTION_RO  ".storage_init"

// Обявление секций
SECTION_DECLARE(STORAGE_SECTION)
SECTION_DECLARE(STORAGE_SECTION_RO)

// Адрес секции
#define STORAGE_LOCATION_RW     __sfb(STORAGE_SECTION)
#define STORAGE_LOCATION_RO     __sfb(STORAGE_SECTION_RO)
// Размер хранилища
#define STORAGE_SIZE            __sfs(STORAGE_SECTION)

// Класс хранилища
static class storage_t : notify_t
{
    // Флаги стостояния
    bool pending, saving;
    
    // Событие заполнения страницы Flash
    class fill_page_event_t : public event_base_t
    {
        // Родительский объект
        storage_t &storage;
    public:
        // Конструктор по умолчанию
        fill_page_event_t(storage_t &parent) : storage(parent)
        { }
        
        // Событие завершения записи
        virtual void notify_event(void)
        {
            FLASH->CR &= ~FLASH_CR_PER;                                         // Page erase end
            // Начало копирования данных из ОЗУ в ПЗУ
            fpec_irq_disable();
                FLASH->CR |= FLASH_CR_PG;                                       // Flash programming
                    auto source = (uint16_t *)STORAGE_LOCATION_RW;
                    auto dest = (volatile uint16_t *)STORAGE_LOCATION_RO;
                    for (auto i = STORAGE_SIZE / sizeof(uint16_t); i > 0; i--, dest++, source ++)
                        *dest = *source;
                    // Ожидаем завершения операции
                    if (!clk_pool(fpec_check_busy, 1))
                        mcu_halt(MCU_HALT_REASON_FLASH);
                    // Проверка ошибок
                    fpec_check_errors();
                FLASH->CR &= ~FLASH_CR_PG;                                      // End flash programming
            fpec_irq_enable();
            fpec_lock();
            // Завершение обновления хранилища
            storage.saving = false;
            if (!storage.pending)
                return;
            // Принудительно обновляем данные снова
            storage.pending = false;
            storage.notify_event();
        }
    } fill_page_event;
    
    // Блокировка FPEC
    INLINE_FORCED
    static void fpec_lock(void)
    {
        FLASH->CR |= FLASH_CR_LOCK;                                             // Set lock bit
    }
    
    // Разблокировка FPEC
    INLINE_FORCED
    static void fpec_unlock(void)
    {
        if (!(FLASH->CR & FLASH_CR_LOCK))                                       // Check lock bit
            return;
        FLASH->KEYR = 0x45670123;                                               // Write first unlock key
        FLASH->KEYR = 0xCDEF89AB;                                               // Write second unlock key
    }

    // Сброс флагов прерывания
    INLINE_FORCED
    static void fpec_irq_clear(void)
    {
        FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR;          // Clear interrupt pending flags
    }
    
    // Проверка ошибок по заврешению операций во Flash
    INLINE_FORCED
    static void fpec_check_errors(void)
    {
        if (FLASH->SR & (FLASH_SR_WRPRTERR | FLASH_SR_PGERR) > 0)
            mcu_halt(MCU_HALT_REASON_FLASH);
        // Сброс флагов прерывания
        fpec_irq_clear();
    }
    
    // Включение прерывания FPEC
    INLINE_FORCED
    static void fpec_irq_enable(void)
    {
        // Сброс флагов прерывания
        fpec_irq_clear();
        // Включение прерывания
        FLASH->CR = FLASH_CR_EOPIE | FLASH_CR_ERRIE;                            // EOP, ERR interrupt enable
    }

    // Отключение прерывания FPEC
    INLINE_FORCED
    static void fpec_irq_disable(void)
    {
        // Отключение прерывания
        FLASH->CR &= ~(FLASH_CR_EOPIE | FLASH_CR_ERRIE);                        // EOP, ERR interrupt disable
    }
    
    // Проверка флага занятости
    static bool fpec_check_busy(void)
    {
        return !(FLASH->SR & FLASH_SR_BSY);                                     // Check busy flag
    }
public:
    // Конструктор по умолчанию
    storage_t(void) : pending(false), saving(false), fill_page_event(*this)
    { }

    // Событие таймера начала записи
    virtual void notify_event(void)
    {
        // Проверка состояния
        if (saving)
        {
            pending = true;
            return;
        }
        // Начало сохранения
        saving = true;
        fpec_unlock();
        FLASH->CR |= FLASH_CR_PER;                                              // Page erase
        FLASH->AR = (uint32_t)STORAGE_LOCATION_RO;                              // Taget page address
        FLASH->CR |= FLASH_CR_STRT;                                             // Start operation
    }
    
    // Инициализация
    INLINE_FORCED
    void init(void)
    {
        // Инициализация модуля FPEC
        fpec_unlock();
            fpec_irq_enable();
        fpec_lock();
        // Инициализация прерывания FPEC
        nvic_irq_enable_set(FLASH_IRQn, true);                                  // Flash interrupt enable
        nvic_irq_priority_set(FLASH_IRQn, NVIC_IRQ_PRIORITY_LOWEST);            // Lowest Flash interrupt priority
    }
    
    // Обработчик завершения стирания Fдash
    INLINE_FORCED
    OPTIMIZE_SPEED
    void erase_done(void)
    {
        // Проверка ошибок
        fpec_check_errors();
        // Генерация события
        event_add(fill_page_event);
    }
    
    // Сигнал изменения данных
    INLINE_FORCED
    void modified(void)
    {
        // 500 мС
        event_timer_start_hz(*this, 2);
    }
} storage;

void storage_init(void)
{
    storage.init();
}

void storage_modified(void)
{
    storage.modified();
}

IRQ_ROUTINE
void storage_interrupt_flash(void)
{
    storage.erase_done();
}
