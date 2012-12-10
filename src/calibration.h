#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <iostream>
#include <stdio.h>
#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "config.h"

class Calibration {
  
public:
    static void withFourPlanes();
};

#endif