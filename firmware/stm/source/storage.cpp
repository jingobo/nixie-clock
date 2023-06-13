#include "mcu.h"
#include "nvic.h"
#include "event.h"
#include "timer.h"
#include "system.h"
#include "storage.h"

// Секция инициализации хранилища (во flash)
#define STORAGE_SECTION_RO  ".storage_init"

// Обявление секций
SECTION_DECL(STORAGE_SECTION)
SECTION_DECL(STORAGE_SECTION_RO)

// Адрес секции
#define STORAGE_LOCATION_RW     __sfb(STORAGE_SECTION)
#define STORAGE_LOCATION_RO     __sfb(STORAGE_SECTION_RO)
// Размер хранилища
#define STORAGE_SIZE            __sfs(STORAGE_SECTION)

// Флаги стостояния
static struct
{
    // В ожидании к сохранению
    bool pending = false;
    // В процессе сохранения
    bool saving = false;
} storage_state;

// Блокировка FPEC
static void storage_fpec_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;                                                 // Set lock bit
}

// Разблокировка FPEC
static void storage_fpec_unlock(void)
{
    if (!(FLASH->CR & FLASH_CR_LOCK))                                           // Check lock bit
        return;
    FLASH->KEYR = 0x45670123;                                                   // Write first unlock key
    FLASH->KEYR = 0xCDEF89AB;                                                   // Write second unlock key
}

// Сброс флагов прерывания
static void storage_fpec_irq_clear(void)
{
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR;              // Clear interrupt pending flags
}

// Проверка ошибок по заврешению операций во Flash
static void storage_fpec_check_errors(void)
{
    if ((FLASH->SR & (FLASH_SR_WRPRTERR | FLASH_SR_PGERR)) != 0)
        mcu_halt(MCU_HALT_REASON_FLASH);
    
    // Сброс флагов прерывания
    storage_fpec_irq_clear();
}

// Включение прерывания FPEC
static void storage_fpec_irq_enable(void)
{
    // Сброс флагов прерывания
    storage_fpec_irq_clear();
    // Включение прерывания
    FLASH->CR = FLASH_CR_EOPIE | FLASH_CR_ERRIE;                                // EOP, ERR interrupt enable
}

// Отключение прерывания FPEC
static void storage_fpec_irq_disable(void)
{
    // Отключение прерывания
    FLASH->CR &= ~(FLASH_CR_EOPIE | FLASH_CR_ERRIE);                            // EOP, ERR interrupt disable
}

// Проверка флага занятости
static bool storage_fpec_check_busy(void)
{
    return (FLASH->SR & FLASH_SR_BSY) == 0;                                     // Check busy flag
}

// Таймер отложенного обновления данных во Flash
static timer_callback_t storage_deffered_flush_timer([](void)
{
    // Проверка состояния
    if (storage_state.saving)
    {
        storage_state.pending = true;
        return;
    }
    
    // Начало сохранения
    storage_state.saving = true;
    storage_fpec_unlock();
    FLASH->CR |= FLASH_CR_PER;                                                  // Page erase
    FLASH->AR = (uint32_t)STORAGE_LOCATION_RO;                                  // Taget page address
    FLASH->CR |= FLASH_CR_STRT;                                                 // Start operation
});

// Событие завершения стирвания страницы Flash
static event_callback_t storage_page_erased_event([](void)
{
    FLASH->CR &= ~FLASH_CR_PER;                                                 // Page erase end
    // Начало копирования данных из ОЗУ в ПЗУ
    storage_fpec_irq_disable();
        FLASH->CR |= FLASH_CR_PG;                                               // Flash programming
            auto source = (uint16_t *)STORAGE_LOCATION_RW;
            auto dest = (volatile uint16_t *)STORAGE_LOCATION_RO;
            for (auto i = STORAGE_SIZE / sizeof(uint16_t); i > 0; i--, dest++, source ++)
                *dest = *source;
            
            // Ожидаем завершения операции
            if (!mcu_pool_ms(storage_fpec_check_busy, 1))
                mcu_halt(MCU_HALT_REASON_FLASH);
            
            // Проверка ошибок
            storage_fpec_check_errors();
        
        FLASH->CR &= ~FLASH_CR_PG;                                              // End flash programming
    storage_fpec_irq_enable();
    storage_fpec_lock();
    
    // Завершение обновления хранилища
    storage_state.saving = false;
    if (!storage_state.pending)
        return;
    storage_state.pending = false;
    
    // Запуск таймера
    storage_modified();
    // Принудительно обновляем данные снова
    storage_deffered_flush_timer.raise();
});

void storage_init(void)
{
    // Инициализация модуля FPEC
    storage_fpec_unlock();
        storage_fpec_irq_enable();
    storage_fpec_lock();
    
    // Инициализация прерывания FPEC
    nvic_irq_enable_set(FLASH_IRQn, true);                                      // Flash interrupt enable
    nvic_irq_priority_set(FLASH_IRQn, NVIC_IRQ_PRIORITY_LOWEST);                // Lowest Flash interrupt priority
}

void storage_modified(void)
{
    // 5 секунд
    storage_deffered_flush_timer.start_us(XM(5));
}

IRQ_ROUTINE
void storage_interrupt_flash(void)
{
    // Проверка ошибок
    storage_fpec_check_errors();
    // Генерация события
    storage_page_erased_event.raise();
}
