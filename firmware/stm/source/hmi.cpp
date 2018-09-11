#include "hmi.h"

void hmi_rgb_t::gamma(const hmi_sat_table_t &table)
{
    r = hmi_gamma_apply(table, r);
    g = hmi_gamma_apply(table, g);
    b = hmi_gamma_apply(table, b);
}

hmi_rgb_t hmi_hsv_t::to_rgb(void) const
{    
  	float_t r, g, b;
	
    float_t h = (float_t)this->h / HMI_SAT_MAX;
	float_t s = (float_t)this->s / HMI_SAT_MAX;
  	float_t v = (float_t)this->v / HMI_SAT_MAX;
	
    uint8_t i = (uint8_t)(h * 6);
    float_t f = h * 6 - i;
    float_t p = v * (1 - s);
    float_t q = v * (1 - f * s);
    float_t t = v * (1 - (1 - f) * s);

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

void hmi_gamma_prepare(hmi_sat_table_t &table, float_t gamma)
{
    for (hmi_sat_t i = 0;; i++)
    {
        table[i] = (hmi_sat_t)(pow((float_t)i / HMI_SAT_MAX, 1 / gamma) * HMI_SAT_MAX);
        if (i >= HMI_SAT_MAX)
            break;
    }
}

hmi_sat_t hmi_gamma_apply(const hmi_sat_table_t &table, hmi_sat_t original)
{
    return table[original];
}
