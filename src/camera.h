#ifndef CAMERA_H
#define CAMERA_H

#define STROBE_0_CNT    0x1500
#define STROBE_1_CNT    0x1504
#define STROBE_2_CNT    0x1508
#define STROBE_3_CNT    0x150C
#define STROBE_CTRL_INQ 0x1300
#define STROBE_0_INQ    0x1400
#define STROBE_1_INQ    0x1404
#define STROBE_2_INQ    0x1408
#define STROBE_3_INQ    0x140C
#define PIO_DIRECTION   0x11F8
#define INITIALIZE      0x000

#include <iostream>
#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <QObject>
#include <QTimer>
#include <QtCore/QTime>
#include <QCoreApplication>

#include "strobe_reg.h"
#include "pio_dir_reg.h"
#include "cam_init_reg.h"
#include <dc1394/dc1394.h>
#include "config.h"

class Camera : public QObject {
    Q_OBJECT

public:
    Camera();
    ~Camera();
    bool open(int deviceIdx);
    void stop();
    void reset();
    void setTestMode(bool toggle);
    void printStatus();
    bool inTestMode();
    int height, width;
    
public slots:
    void start();

private slots:
    void captureFrame();

signals:
    void newFrame(cv::Mat frame);
    void stopped();

private:
    int numCams;
    int numDMABuffers;
    dc1394camera_t *camera;
    dc1394_t *camDict;
    dc1394camera_list_t *camList;
    dc1394error_t error;

    QTimer *eventLoopTimer;
    std::vector<cv::Mat> testImages;
    bool testMode;
    int imgIdx;
    int FRAME_RATE;
    int camFrameWidth, camFrameHeight;

    /* typedefs for writing in register */
    typedef strobe_cnt_reg<uint32_t> strobe_cnt_reg32;
    typedef strobe_inq_reg<uint32_t> strobe_inq_reg32;
    typedef strobe_ctrl_inq_reg<uint32_t> strobe_ctrl_inq_reg32;
    typedef pio_dir_reg<uint32_t> pio_dir_reg32;
    typedef cam_ini_reg<uint32_t> cam_init_reg32;

    void resetCamera();
    void startResetPulse();
    void stopResetPulse();
    void startClockPulse();
    void stopClockPulse();
    /** Get the control register value of the camera at given offset */
    uint32_t readRegisterContent(uint64_t offset);
    /** Set control register value of camera at given offset to given value */
    void writeRegisterContent(uint64_t offset, uint32_t value);
    /** Set first two GPIO pins as ouput. PIO_DIRECTION register 0x11F8. Bit 0-3
     * represent GPIO0-GPIO3. 0 input, 1 output */
    void configureOutputPins();
    /** Set clock delay. STROBE_0_CNT register 0x1500 */
    void configureClockDelay();
    /** Set clock duration. STROBE_0_CNT register 0x1500 */
    void configureClockDuration();
    /** Set reset delay. STROBE_1_CNT register 0x1504 */
    void configureResetDelay();
    /** Set reset duration. STROBE_1_CNT register 0x1504 */
    void configureResetDuration();
    /** Closing camera and cleaning up */
    void cleanup(dc1394camera_t *camera);
    /* Own implementation of sleep, processing all qt events while sleeping/waiting */
    void msleep(unsigned long msecs);
};

#endif
