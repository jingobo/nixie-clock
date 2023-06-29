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

// Ожидание завершения операции
RAM_IAR
static void storage_fpec_wait(void)
{
    // Ожидание занятости
    while ((FLASH->SR & FLASH_SR_BSY) != 0)                                     // Check busy flag
    { }
    
    // Проверка ошибок по заврешению операций во Flash
    if ((FLASH->SR & (FLASH_SR_WRPRTERR | FLASH_SR_PGERR)) != 0)
        mcu_halt(MCU_HALT_REASON_FLASH);
    
    // Сброс флагов прерывания
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR;              // Clear interrupt pending flags
}

// Обновляет данные хранилища во Flash
RAM_IAR
static void storage_flash_update(void)
{
    // Разблокировка FPEC
    FLASH->KEYR = 0x45670123;                                                   // Write first unlock key
    FLASH->KEYR = 0xCDEF89AB;                                                   // Write second unlock key

        // Стирание страницы
        FLASH->CR |= FLASH_CR_PER;                                              // Page erase
        {
            FLASH->AR = (uint32_t)__sfb(STORAGE_SECTION_RO);                    // Taget page address
            FLASH->CR |= FLASH_CR_STRT;                                         // Start operation
            
            // Ожидаем завершения операции
            storage_fpec_wait();
        }
        FLASH->CR &= ~FLASH_CR_PER;                                             // Page erase end

        // Программирование
        FLASH->CR |= FLASH_CR_PG;                                               // Flash programming
        {
            // Размер секции хранилища
            static volatile size_t SIZE = __sfs(STORAGE_SECTION);
            
            // Источник и приёмник данных
            auto source = (uint16_t *)__sfb(STORAGE_SECTION);
            auto dest = (volatile uint16_t *)__sfb(STORAGE_SECTION_RO);
            
            // Копирование
            for (auto i = SIZE / sizeof(uint16_t); i > 0; i--, dest++, source ++)
                *dest = *source;
            
            // Ожидаем завершения операции
            storage_fpec_wait();
        }
        FLASH->CR &= ~FLASH_CR_PG;                                              // End flash programming
    
    // Блокировка FPEC
    FLASH->CR |= FLASH_CR_LOCK;                                                 // Set lock bit
}

// Таймер отложенного обновления данных во Flash
static timer_t storage_deffered_flush_timer([](void)
{
    // Маскирование прерываний до нормального приоритета включительно
    nvic_irq_priority_mask(NVIC_IRQ_PRIORITY_DEFAULT);

        storage_flash_update();

    // Размаскирование всех прерываний
    nvic_irq_priority_mask(NVIC_IRQ_PRIORITY_HIGHEST);
});

void storage_modified(void)
{
    // 5 секунд
    storage_deffered_flush_timer.start_us(XM(5));
}
