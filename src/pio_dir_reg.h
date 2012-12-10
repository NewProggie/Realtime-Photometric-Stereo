#ifndef PIO_DIR_REG_H
#define PIO_DIR_REG_H

#define __REVERSE_BITS__

template <typename base_type>
union pio_dir_reg {
    base_type value;
#ifdef __REVERSE_BITS__
    struct {
        base_type          : 28;
        base_type io3_mode : 1;
        base_type io2_mode : 1;
        base_type io1_mode : 1;
        base_type io0_mode : 1;
    };
#else
    struct {
        base_type io0_mode : 1;
        base_type io1_mode : 1;
        base_type io2_mode : 1;
        base_type io3_mode : 1;
        base_type          : 28;
    };
#endif
    inline operator base_type() const { return value; }
    pio_dir_reg(const base_type& arg) { value = arg; }
};

#endif