﻿#include "hmi.h"

// Таблица коррекции гаммы (g = 2.2)
// f(x) = (x / 255) ^ 2.2 * 255
const hmi_sat_table_t HMI_GAMMA_TABLE =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  
    3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  
    6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 11, 11, 11, 
    12, 12, 13, 13, 14, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 
    19, 20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 
    29, 30, 30, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 39, 39, 40, 
    41, 42, 43, 43, 44, 45, 46, 47, 48, 49, 50, 50, 51, 52, 53, 54, 
    55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 71, 
    72, 73, 74, 75, 76, 77, 78, 80, 81, 82, 83, 84, 86, 87, 88, 89, 
    91, 92, 93, 94, 96, 97, 98,100,101,102,104,105,106,108,109,110, 
    112,113,115,116,118,119,121,122,123,125,126,128,130,131,133,134,
    136,137,139,140,142,144,145,147,149,150,152,154,155,157,159,160,
    162,164,166,167,169,171,173,175,176,178,180,182,184,186,187,189,
    191,193,195,197,199,201,203,205,207,209,211,213,215,217,219,221,
    223,225,227,229,231,233,235,238,240,242,244,246,248,251,253,255
};

// Цвета HSV
const hmi_hsv_t 
    HMI_COLOR_HSV_BLACK,
    HMI_COLOR_HSV_RED(HMI_SAT_MIN),
    HMI_COLOR_HSV_GREEN(85),
    HMI_COLOR_HSV_BLUE(170),
    HMI_COLOR_HSV_WHITE(HMI_SAT_MIN, HMI_SAT_MIN, HMI_SAT_MAX);

// Цвета RGB
const hmi_rgb_t 
    HMI_COLOR_RGB_BLACK,
    HMI_COLOR_RGB_RED(HMI_SAT_MAX, HMI_SAT_MIN, HMI_SAT_MIN),
    HMI_COLOR_RGB_GREEN(HMI_SAT_MIN, HMI_SAT_MAX, HMI_SAT_MIN),
    HMI_COLOR_RGB_BLUE(HMI_SAT_MIN, HMI_SAT_MIN, HMI_SAT_MAX),
    HMI_COLOR_RGB_WHITE(HMI_SAT_MAX, HMI_SAT_MAX, HMI_SAT_MAX);

hmi_rgb_t hmi_hsv_t::to_rgb(void) const
{
    auto h = (float_t)this->h / HMI_SAT_MAX;
    auto s = (float_t)this->s / HMI_SAT_MAX;
    auto v = (float_t)this->v / HMI_SAT_MAX;
    
    auto i = (uint8_t)(h * 6);
    auto f = h * 6 - i;
    auto p = v * (1 - s);
    auto q = v * (1 - f * s);
    auto t = v * (1 - (1 - f) * s);
    
    float_t r, g, b;
    switch (i % 6)
    {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }

    return hmi_rgb_t(
        (hmi_sat_t)(r * HMI_SAT_MAX),
        (hmi_sat_t)(g * HMI_SAT_MAX),
        (hmi_sat_t)(b * HMI_SAT_MAX));
}
