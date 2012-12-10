#ifndef STROBE_REG_H
#define STROBE_REG_H

#define __REVERSE_BITS__

template <typename base_type>
union strobe_cnt_reg {
    base_type value;
#ifdef __REVERSE_BITS__
    struct {
        base_type duration_value  : 12;
        base_type delay_value     : 12;
        base_type signal_polarity : 1;
        base_type on_off          : 1;
        base_type                 : 5;
        base_type presence_inq    : 1;
    };
#else
    struct {
        base_type presence_inq    : 1;
        base_type                 : 5;
        base_type on_off          : 1;
        base_type signal_polarity : 1;
        base_type delay_value     : 12;
        base_type duration_value  : 12;
    };
#endif
    inline operator base_type() const { return value; }
    strobe_cnt_reg(const base_type& arg) { value = arg; }
};

template <typename base_type>
union strobe_ctrl_inq_reg {
    base_type value;
#ifdef __REVERSE_BITS__
    struct {
        base_type              : 28;
        base_type strobe_3_inq : 1;
        base_type strobe_2_inq : 1;
        base_type strobe_1_inq : 1;
        base_type strobe_0_inq : 1;
    };
#else
    struct {
        base_type strobe_0_inq : 1;
        base_type strobe_1_inq : 1;
        base_type strobe_2_inq : 1;
        base_type strobe_3_inq : 1;
        base_type              : 28;
    };
#endif
    inline operator base_type() const { return value; }
    strobe_ctrl_inq_reg(const base_type& arg) { value = arg; }
};

template <typename base_type>
union strobe_inq_reg {
    base_type value;
#ifdef __REVERSE_BITS__
    struct {
        base_type max_value    : 12;
        base_type min_value    : 12;
        base_type              : 1;
        base_type polarity_inq : 1;
        base_type on_off_inq   : 1;
        base_type readout_inq  : 1;
        base_type              : 3;
        base_type presence_inq : 1;
    };
#else
    struct {
        base_type presence_inq : 1;
        base_type              : 3;
        base_type readout_inq  : 1;
        base_type on_off_inq   : 1;
        base_type polarity_inq : 1;
        base_type              : 1;
        base_type min_value    : 12;
        base_type max_value    : 12;
    };
#endif
    inline operator base_type() const { return value; }
    strobe_inq_reg(const base_type& arg) { value = arg; }
};

#endif
