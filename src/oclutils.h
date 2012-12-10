#ifndef OCLUTILS_H
#define OCLUTILS_H

#include <stdio.h>

#include "OpenCL/cl.hpp"

class OCLUtils {
  
public:
    static char *fileContents(const char *filename, int *length);
    static const char *oclErrorString(cl_int error);
};

#endif