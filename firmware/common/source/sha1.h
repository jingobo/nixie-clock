#ifndef SHA1_H
#define SHA1_H

#include "common.h"

// Размер хзша в байтах
#define SHA1_HASH_SIZE      20
// Размер одного блока
#define SHA1_BLOCK_SIZE     64

// Класс хэширования в SHA-1
class sha1_t
{
    size_t count[2];
    uint32_t state[5];
    uint8_t buffer[SHA1_BLOCK_SIZE];
    union
    {
        uint8_t c[SHA1_BLOCK_SIZE];
        uint32_t l[SHA1_BLOCK_SIZE / 4];
    } block;
    
    // трансформация состояния
    void transform(const uint8_t *buffer);
public:
    // Сброс перед новым хэшированием
    void reset(void);

    // Добавление данных к хэшированию
    void update(const char *source, size_t size);
    
    // Завершение хэширования и получение хэша
    char * final(void);
};

#endif // SHA1_H
