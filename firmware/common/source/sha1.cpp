// Портировано с репрозитория https://github.com/clibs/sha1

#include "sha1.h"

// LITTLE_ENDIAN
#define SHA1_BLK0(i)            (block.l[i] = (SHA1_ROL(block.l[i], 24) & 0xFF00FF00) | (SHA1_ROL(block.l[i], 8) & 0x00FF00FF))
// BIG_ENDIAN
// #define SHA1_BLK0(i)         block.l[i]

#define SHA1_ROL(v, bits)       (((v) << (bits)) | ((v) >> (32 - (bits))))
#define SHA1_BLK(i)             (block.l[i & 15] = SHA1_ROL(block.l[(i + 13) & 15] ^ block.l[(i + 8) & 15] ^ block.l[(i + 2) & 15] ^ block.l[i & 15], 1))

#define SHA1_R0(v,w,x,y,z,i)    z += ((w &(x ^ y)) ^ y) + SHA1_BLK0(i) + 0x5A827999 + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30)
#define SHA1_R1(v,w,x,y,z,i)    z += ((w &(x ^ y)) ^ y) + SHA1_BLK(i) + 0x5A827999 + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30)
#define SHA1_R2(v,w,x,y,z,i)    z += (w ^ x ^ y) + SHA1_BLK(i) + 0x6ED9EBA1 + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30)
#define SHA1_R3(v,w,x,y,z,i)    z += (((w | x) & y) | (w & x)) + SHA1_BLK(i) + 0x8F1BBCDC + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30)
#define SHA1_R4(v,w,x,y,z,i)    z += (w ^ x ^ y) + SHA1_BLK(i) + 0xCA62C1D6 + SHA1_ROL(v, 5); w = SHA1_ROL(w, 30)

void sha1_t::transform(const uint8_t *buffer)
{
    memcpy(&block, buffer, sizeof(block));

    /* Копирование текущих состояний */
    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    auto e = state[4];
    /* 4 раунда по 20 операций */
    SHA1_R0(a, b, c, d, e, 0);  SHA1_R0(e, a, b, c, d, 1);  SHA1_R0(d, e, a, b, c, 2);  SHA1_R0(c, d, e, a, b, 3);
    SHA1_R0(b, c, d, e, a, 4);  SHA1_R0(a, b, c, d, e, 5);  SHA1_R0(e, a, b, c, d, 6);  SHA1_R0(d, e, a, b, c, 7);
    SHA1_R0(c, d, e, a, b, 8);  SHA1_R0(b, c, d, e, a, 9);  SHA1_R0(a, b, c, d, e, 10); SHA1_R0(e, a, b, c, d, 11);
    SHA1_R0(d, e, a, b, c, 12); SHA1_R0(c, d, e, a, b, 13); SHA1_R0(b, c, d, e, a, 14); SHA1_R0(a, b, c, d, e, 15);
    SHA1_R1(e, a, b, c, d, 16); SHA1_R1(d, e, a, b, c, 17); SHA1_R1(c, d, e, a, b, 18); SHA1_R1(b, c, d, e, a, 19);
    SHA1_R2(a, b, c, d, e, 20); SHA1_R2(e, a, b, c, d, 21); SHA1_R2(d, e, a, b, c, 22); SHA1_R2(c, d, e, a, b, 23);
    SHA1_R2(b, c, d, e, a, 24); SHA1_R2(a, b, c, d, e, 25); SHA1_R2(e, a, b, c, d, 26); SHA1_R2(d, e, a, b, c, 27);
    SHA1_R2(c, d, e, a, b, 28); SHA1_R2(b, c, d, e, a, 29); SHA1_R2(a, b, c, d, e, 30); SHA1_R2(e, a, b, c, d, 31);
    SHA1_R2(d, e, a, b, c, 32); SHA1_R2(c, d, e, a, b, 33); SHA1_R2(b, c, d, e, a, 34); SHA1_R2(a, b, c, d, e, 35);
    SHA1_R2(e, a, b, c, d, 36); SHA1_R2(d, e, a, b, c, 37); SHA1_R2(c, d, e, a, b, 38); SHA1_R2(b, c, d, e, a, 39);
    SHA1_R3(a, b, c, d, e, 40); SHA1_R3(e, a, b, c, d, 41); SHA1_R3(d, e, a, b, c, 42); SHA1_R3(c, d, e, a, b, 43);
    SHA1_R3(b, c, d, e, a, 44); SHA1_R3(a, b, c, d, e, 45); SHA1_R3(e, a, b, c, d, 46); SHA1_R3(d, e, a, b, c, 47);
    SHA1_R3(c, d, e, a, b, 48); SHA1_R3(b, c, d, e, a, 49); SHA1_R3(a, b, c, d, e, 50); SHA1_R3(e, a, b, c, d, 51);
    SHA1_R3(d, e, a, b, c, 52); SHA1_R3(c, d, e, a, b, 53); SHA1_R3(b, c, d, e, a, 54); SHA1_R3(a, b, c, d, e, 55);
    SHA1_R3(e, a, b, c, d, 56); SHA1_R3(d, e, a, b, c, 57); SHA1_R3(c, d, e, a, b, 58); SHA1_R3(b, c, d, e, a, 59);
    SHA1_R4(a, b, c, d, e, 60); SHA1_R4(e, a, b, c, d, 61); SHA1_R4(d, e, a, b, c, 62); SHA1_R4(c, d, e, a, b, 63);
    SHA1_R4(b, c, d, e, a, 64); SHA1_R4(a, b, c, d, e, 65); SHA1_R4(e, a, b, c, d, 66); SHA1_R4(d, e, a, b, c, 67);
    SHA1_R4(c, d, e, a, b, 68); SHA1_R4(b, c, d, e, a, 69); SHA1_R4(a, b, c, d, e, 70); SHA1_R4(e, a, b, c, d, 71);
    SHA1_R4(d, e, a, b, c, 72); SHA1_R4(c, d, e, a, b, 73); SHA1_R4(b, c, d, e, a, 74); SHA1_R4(a, b, c, d, e, 75);
    SHA1_R4(e, a, b, c, d, 76); SHA1_R4(d, e, a, b, c, 77); SHA1_R4(c, d, e, a, b, 78); SHA1_R4(b, c, d, e, a, 79);
    /* Обновление состояний */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void sha1_t::reset(void)
{
    state[0] = 0x67452301;
    state[1] = 0xEFCDAB89;
    state[2] = 0x98BADCFE;
    state[3] = 0x10325476;
    state[4] = 0xC3D2E1F0;
    count[0] = count[1] = 0;
}

void sha1_t::update(const char *source, size_t size)
{
    size_t i = 0;
    auto j = count[0];
    if ((count[0] += size << 3) < j)
        count[1]++;
    count[1] += size >> 29;
    j = (j >> 3) & 63;
    if ((j + size) > 63)
    {
        memcpy(buffer + j, source, (i = 64 - j));
        transform(buffer);
        for (; i + 63 < size; i += 64)
            transform((uint8_t *)(source + i));
        j = 0;
    }
    memcpy(buffer + j, source + i, size - i);
}

char * sha1_t::final(void)
{
    char finalcount[8];
    for (auto i = 0; i < sizeof(finalcount); i++)
        finalcount[i] = (char)((count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    {
        auto c = (char)128;
        update(&c, 1);
        for (c = 0; (count[0] & 504) != 448;)
            update(&c, 1);
    }
    update(finalcount, sizeof(finalcount));
    for (auto i = 0; i < SHA1_HASH_SIZE; i++)
        buffer[i] = (uint8_t)((state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    buffer[SHA1_HASH_SIZE] = 0;
    return (char *)buffer;
}
