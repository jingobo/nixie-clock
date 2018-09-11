#ifndef __HMI_H
#define __HMI_H

#include "typedefs.h"
#include <list.h>

// ���������� �������� �������
#define HMI_RANK_COUNT          6
// ������ � ������� (��)
#define HMI_FRAME_RATE          100

// ����������� ������������
#define HMI_SAT_MIN             0
// ������������ ������������
#define HMI_SAT_MAX             UINT8_MAX
// ���������� ������� ������������
#define HMI_SAT_COUNT           (1U + HMI_SAT_MAX)

// ASCII ��������
#define HMI_CHAR_ZERO           '0'
#define HMI_CHAR_NINE           '9'
#define HMI_CHAR_SPACE          ' '
#define HMI_CHAR_DOT            '.'

// ��� ������ ��� ���������� �������
typedef uint8_t hmi_rank_t;
// ��� ������ ��� �������� ������������ ����������
typedef uint8_t hmi_sat_t;
// ������� ������� ������������
typedef hmi_sat_t hmi_sat_table_t[HMI_SAT_COUNT];

// ��� ��� �������� ����� � ������� RGB
ALIGN_FIELD_8
union hmi_rgb_t
{
    uint32_t raw;
    struct
    {
        hmi_sat_t g;
        hmi_sat_t r;
        hmi_sat_t b;
        // ��� ������������ (�� ������������)
        hmi_sat_t a;
    };
    uint8_t grba[4];
    
    // ������������
    hmi_rgb_t(void) : hmi_rgb_t(0)
    { }
    hmi_rgb_t(uint32_t _raw) : raw(_raw) 
    { }
    hmi_rgb_t(hmi_sat_t _r, hmi_sat_t _g, hmi_sat_t _b) : r(_r), g(_g), b(_b), a(0) 
    { }

    // ���������
    bool operator == (const hmi_rgb_t& a) const
    { 
        return raw == a.raw; 
    }

    // ���������� ����� ���������
    void gamma(const hmi_sat_table_t &table);
};
ALIGN_FIELD_DEF

// ��� ��� �������� ����� � ������� HSV
ALIGN_FIELD_8
union hmi_hsv_t
{
    uint32_t raw;
    struct
    {
        hmi_sat_t h;
        hmi_sat_t s;
        hmi_sat_t v;
        // ��� ������������ (�� ������������)
        hmi_sat_t a;
    };
    uint8_t hsva[4];
    
    // ������������
    hmi_hsv_t(void) : hmi_hsv_t(0)
    { }
    hmi_hsv_t(uint32_t _raw) : raw(_raw) 
    { }
    hmi_hsv_t(hmi_sat_t _h, hmi_sat_t _s, hmi_sat_t _v) : h(_h), s(_s), v(_v), a(0) 
    { }
    
    // ���������
    bool operator == (const hmi_hsv_t& a) const
    { 
        return raw == a.raw;
    }
    
    // ��������������� HSV � RGB
    hmi_rgb_t to_rgb(void) const;
};
ALIGN_FIELD_DEF

// ���������� ����������� ������
enum hmi_filter_purpose_t
{
    // ������� ������� ��������
    HMI_FILTER_PURPOSE_CHAIN = 0,
    // ��������� ���������� (�������� ������)
    HMI_FILTER_PURPOSE_CONTROLLER,
    
    // ������� ��������� ������������
    HMI_FILTER_PURPOSE_SMOOTH_SAT,
    // ������� ��������� ��������
    HMI_FILTER_PURPOSE_SMOOTH_VAL,
    
    // ���������� ��������� ������������
    HMI_FILTER_PURPOSE_INSTANT_SAT,
    // ���������� ��������� ��������
    HMI_FILTER_PURPOSE_INSTANT_VAL,
    
    // ��������� �����
    HMI_FILTER_PURPOSE_GAMMA,
    // �������� ������� (������ ������)
    HMI_FILTER_PURPOSE_DISPLAY
};


// ������������ ������ ����� �������
template <typename DATA, typename PARTS>
class hmi_filter_data_t
{
    // 
    DATA data;
    
};


// ������� ����� ��������
template <typename DATA, hmi_rank_t COUNT>
class hmi_filter_t : list_item_t
{
    // ��������� ���������� ������
    DATA last[COUNT];
    
    // �������� �������
    static void index_check(hmi_rank_t index)
    { assert(index < COUNT); }
protected:    
    // ������� ����� ������
    virtual void do_data_set(hmi_rank_t index, DATA &data)
    { }
    // ������� ����������
    virtual void do_refresh(void)
    { }
    
    // �������� ����������� ���������� �� ��������� ������
    hmi_filter_t<DATA, COUNT> * next_cast(void) const
    {
        return static_cast<hmi_filter_t<DATA, COUNT> *>(next());
    }
    // ���������� �������� ������ ���������� �������
    void next_data_set(hmi_rank_t index, DATA data) const
    {
        auto next = next_cast();
        if (next != NULL)
            next->data_set(index, data);
    }
public:
    // �������� ����������
    const hmi_filter_purpose_t purpose;
    
    // ������� �����������
    hmi_filter_t(hmi_filter_purpose_t _purpose) : purpose(_purpose)
    { }
    
    // �������� ������� ������
    DATA data_get(hmi_rank_t index) const
    { 
        index_check(index);
        return last[index]; 
    }
    // ������������� ����� �����
    void data_set(hmi_rank_t index, DATA data)
    {
        index_check(index);
        // ���������� �� ������
        if (last[index] == data)
            return;
        last[index] = data;
        // ����� ����������� ����� ������
        do_data_set(index, data);
        // ��������� ������� �������� � ������� � ���������� �������
        next_data_set(index, data);
    }
    // ��������� ������ (������������)
    void refresh(void)
    {
        do_refresh();
        // ����� ������ ���������� � ���������� �������
        auto next = next_cast();
        if (next != NULL)
            next->refresh();        
    }
    // ���������� ������� � �������
    void attach(hmi_filter_t &item)
    {
        // �������� ����������
        assert(item.purpose > purpose);
        // ����������
        auto next = next_cast();
        // �� ����� �������� ����� �������� � ������� ���������...
        if (next != NULL && next->purpose < item.purpose)
        {
            next->attach(item);
            return;
        }
        // ������� ����� ������� � ���������
        item.link(this, LIST_SIDE_NEXT);
    }
    // �������� ������� �� �������
    bool detach(hmi_filter_t &item)
    {
        // �������� ����������
        assert(&item != this);
        // ����������
        item.unlink();
        return true;
        /*auto next = next_cast();
        if (next == NULL)
            return false;
        // ����� ������
        if (next == &item)
        {
            item.remove(this);
            return true;
        }
        return next->detach(item);*/
    }
};

// ������� ��������
template <typename DATA, hmi_rank_t COUNT>
struct hmi_filter_chain_t : hmi_filter_t<DATA, COUNT>
{
    // ����������� �� ���������
    hmi_filter_chain_t(void) : hmi_filter_t<DATA, COUNT>(HMI_FILTER_PURPOSE_CHAIN)
    { }
};

// ���������� ������� ����� ���������
void hmi_gamma_prepare(hmi_sat_table_t &table, float_t gamma = 0.45f);
// ���������� ����� ���������
hmi_sat_t hmi_gamma_apply(const hmi_sat_table_t &table, hmi_sat_t original);

// ������� ������ �����
template <typename DATA, hmi_rank_t COUNT>
class hmi_filter_gamma_t : public hmi_filter_t<DATA, COUNT>
{
    // ������� �������� ����������� �����
    float_t gamma;
protected:
    // ������� ��������� �����
    hmi_sat_table_t table;
public:
    // ����������� �� ���������
    hmi_filter_gamma_t(void) : hmi_filter_t<DATA, COUNT>(HMI_FILTER_PURPOSE_GAMMA), gamma(0.0f)
    { }
    
    // ������������� ����� �������� ���������
    void gamma_set(float_t value = 0.45f)
    {
        // �������� ����������
        assert(value > 0.0f);
        // �������� �� ������������� ���������
        if (gamma == value)
            return;
        gamma = value;
        hmi_gamma_prepare(table, gamma);
    }
    // �������� ������� �������� ���������
    float_t gamma_get(void)
    {
        return gamma;
    }
};

#endif // __HMI_H
