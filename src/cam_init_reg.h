#ifndef CAM_INIT_REG_H
#define CAM_INIT_REG_H

#define __REVERSE_BITS__

template <typename base_type>
union cam_ini_reg {
    base_type value;
#ifdef __REVERSE_BITS__
    struct {
        base_type      : 30;
        base_type init : 1;
    };
#else
    struct {
        base_type init : 1;
        base_type      : 30;
    };
#endif
    inline operator base_type() const { return value; }
    cam_ini_reg(const base_type& arg) { value = arg;}
};

#endif