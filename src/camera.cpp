#include "camera.h"

Camera::Camera() {

    camDict = dc1394_new();
    error = dc1394_camera_enumerate(camDict, &camList);
    if (error != DC1394_SUCCESS) {
        std::cerr << "Failed to enumerate cameras." << std::endl;
        exit(EXIT_FAILURE);
    }

    numCams = camList->num;
    numDMABuffers = 3;
    FRAME_RATE = 15;
    /* minimum resolution of camera */
    camFrameHeight = IMG_HEIGHT;
    camFrameWidth = IMG_WIDTH;
    /* we crop images to quadratic dimensions for evenly divisable work-group items */
    height = width = camFrameHeight;

    /* image index iterating modulo 8 */
    imgIdx = START_LED;
    
    /* loading images for test mode */
    for (int i=0; i<8; i++) {
        std::stringstream s;
        s << PATH_ASSETS << "hand/image" << i << ".png";
        cv::Mat img = cv::imread(s.str(), 0);
        testImages.push_back(img);
    }

    /* counter iterating over (modulo 8) LEDs assigning image to current LED */
    imgIdx = 3;
    testMode = false;
}

Camera::~Camera() {

    stop();
}

bool Camera::open(int deviceIdx) {

    if (numCams < 1) {
        std::cerr << "Camera not found or could not be opened." << std::endl;
        return false;
    }

    camera = dc1394_camera_new(camDict, camList->ids[deviceIdx].guid);
    if (!camera) {
        dc1394_log_error("Failed to initialize camera with guid %.", camList->ids[deviceIdx].guid);
        return false;
    }

    /* reset camera to initial state with default values */
    resetCamera();

    /* prepare camera for ringlight leds */
    configureOutputPins();
    configureClockDelay();
    configureClockDuration();
    configureResetDelay();
    configureResetDuration();

    /* set video mode */
    error = dc1394_video_set_mode(camera,  DC1394_VIDEO_MODE_640x480_MONO8);
    if (error != DC1394_SUCCESS) {
        std::cout << "Could not set video mode" << std::endl;
        cleanup(camera);
        return false;
    }

    /* camera parameter calibrated with FlyCapture2 */

    /* brightness */
    error = dc1394_feature_set_mode(camera, DC1394_FEATURE_BRIGHTNESS, DC1394_FEATURE_MODE_MANUAL);
    error = dc1394_feature_set_value(camera, DC1394_FEATURE_BRIGHTNESS, 0);

    /* exposure */
    error = dc1394_feature_set_mode(camera, DC1394_FEATURE_EXPOSURE, DC1394_FEATURE_MODE_MANUAL);
    error = dc1394_feature_set_value(camera, DC1394_FEATURE_EXPOSURE, 149);

    /* gamma */
    error = dc1394_feature_set_mode(camera, DC1394_FEATURE_GAMMA, DC1394_FEATURE_MODE_MANUAL);
    error = dc1394_feature_set_value(camera, DC1394_FEATURE_GAMMA, 1024);

    /* shutter */
    error = dc1394_feature_set_mode(camera, DC1394_FEATURE_SHUTTER, DC1394_FEATURE_MODE_MANUAL);
    error = dc1394_feature_set_value(camera, DC1394_FEATURE_SHUTTER, 61);

    /* gain */
    error = dc1394_feature_set_mode(camera, DC1394_FEATURE_GAIN, DC1394_FEATURE_MODE_MANUAL);
    error = dc1394_feature_set_value(camera, DC1394_FEATURE_GAIN, 740);

    /* frame rate */
    error = dc1394_feature_set_mode(camera, DC1394_FEATURE_FRAME_RATE, DC1394_FEATURE_MODE_MANUAL);
    error = dc1394_feature_set_absolute_value(camera, DC1394_FEATURE_FRAME_RATE, FRAME_RATE);

    /* setup capture */
    error = dc1394_capture_setup(camera, numDMABuffers, DC1394_CAPTURE_FLAGS_DEFAULT);
    if (error != DC1394_SUCCESS) {
        std::cerr << "Could not setup camera-\nmake sure that the video mode and framerate are\nsupported by your camera" << std::endl;
        cleanup(camera);
        return false;
    }

    /* starting camera sending data */
    error = dc1394_video_set_transmission(camera, DC1394_ON);
    if (error != DC1394_SUCCESS) {
        std::cerr << "Could not start camera iso transmission" << std::endl;
        cleanup(camera);
        return false;
    }

    return true;
}

void Camera::start() {

    /* starting event loop, capturing fresh images */
    eventLoopTimer = new QTimer();
    connect(eventLoopTimer, SIGNAL(timeout()), this, SLOT(captureFrame()));
    eventLoopTimer->start();
}

void Camera::stop() {

    eventLoopTimer->stop();
    stopClockPulse();
    dc1394_video_set_transmission(camera, DC1394_OFF);
    dc1394_capture_stop(camera);
    dc1394_camera_free(camera);
    dc1394_free (camDict);
    
    emit stopped();
}

void Camera::setTestMode(bool toggle) {
    testMode = toggle;
}

bool Camera::inTestMode() {
    return testMode;
}

void Camera::printStatus() {

    float val;
    std::cout << "CAMERA STATUS: " << std::endl;

    dc1394_feature_get_absolute_value(camera, DC1394_FEATURE_FRAME_RATE, &val);
    std::cout << "  framerate : " << val << " fps" << std::endl;

    uint32_t uval32;
    dc1394_feature_get_value(camera, DC1394_FEATURE_SHUTTER, &uval32);
    std::cout << "  shutter  : " << uval32 << "\n" << std::endl;

    uint64_t pio_dir_addr = PIO_DIRECTION;
    pio_dir_reg32 pio_dir_reg = readRegisterContent(pio_dir_addr);
    std::cout << "  gpio 0 as output configured : " << pio_dir_reg.io0_mode << std::endl;
    std::cout << "  gpio 1 as output configured : " << pio_dir_reg.io1_mode << std::endl;
    std::cout << "  gpio 2 as output configured : " << pio_dir_reg.io2_mode << std::endl;
    std::cout << "  gpio 3 as output configured : " << pio_dir_reg.io3_mode << "\n" << std::endl;

    uint64_t strobe_ctrl_inq_addr = STROBE_CTRL_INQ;
    strobe_ctrl_inq_reg32 strobe_ctrl_ing_reg = readRegisterContent(strobe_ctrl_inq_addr);
    std::cout << "  strobe 0 present : " << strobe_ctrl_ing_reg.strobe_0_inq << std::endl;
    std::cout << "  strobe 1 present : " << strobe_ctrl_ing_reg.strobe_1_inq << std::endl;
    std::cout << "  strobe 2 present : " << strobe_ctrl_ing_reg.strobe_2_inq << std::endl;
    std::cout << "  strobe 3 present : " << strobe_ctrl_ing_reg.strobe_3_inq << "\n" << std::endl;

    uint64_t strobe_0_inq_addr = STROBE_0_INQ;
    strobe_inq_reg32 strobe_0_inq_reg = readRegisterContent(strobe_0_inq_addr);
    std::cout << "  strobe_0_inq presence_inq    : " << strobe_0_inq_reg.presence_inq << std::endl;
    std::cout << "  strobe_0_inq readout_inq     : " << strobe_0_inq_reg.readout_inq << std::endl;
    std::cout << "  strobe_0_inq on_off_inq      : " << strobe_0_inq_reg.on_off_inq << std::endl;
    std::cout << "  strobe_0_inq polarity_inq    : " << strobe_0_inq_reg.polarity_inq << std::endl;
    std::cout << "  strobe_0_inq min_value       : " << strobe_0_inq_reg.min_value << std::endl;
    std::cout << "  strobe_0_inq max_value       : " << strobe_0_inq_reg.max_value << "\n" << std::endl;

    uint64_t strobe_1_inq_addr = STROBE_1_INQ;
    strobe_inq_reg32 strobe_1_inq_reg = readRegisterContent(strobe_1_inq_addr);
    std::cout << "  strobe_1_inq presence_inq    : " << strobe_1_inq_reg.presence_inq << std::endl;
    std::cout << "  strobe_1_inq readout_inq     : " << strobe_1_inq_reg.readout_inq << std::endl;
    std::cout << "  strobe_1_inq on_off_inq      : " << strobe_1_inq_reg.on_off_inq << std::endl;
    std::cout << "  strobe_1_inq polarity_inq    : " << strobe_1_inq_reg.polarity_inq << std::endl;
    std::cout << "  strobe_1_inq min_value       : " << strobe_1_inq_reg.min_value << std::endl;
    std::cout << "  strobe_1_inq max_value       : " << strobe_1_inq_reg.max_value << "\n" << std::endl;

    uint64_t strobe_0_cnt_addr = STROBE_0_CNT;
    strobe_cnt_reg32 strobe_0_cnt_reg = readRegisterContent(strobe_0_cnt_addr);
    std::cout << "  strobe_0_cnt presence_inq    : " << strobe_0_cnt_reg.presence_inq << std::endl;
    std::cout << "  strobe_0_cnt on_off          : " << strobe_0_cnt_reg.on_off << std::endl;
    std::cout << "  strobe_0_cnt signal_polarity : " << strobe_0_cnt_reg.signal_polarity << std::endl;
    std::cout << "  strobe_0_cnt delay_value     : " << strobe_0_cnt_reg.delay_value << std::endl;
    std::cout << "  strobe_0_cnt duration_value  : " << strobe_0_cnt_reg.duration_value << "\n" << std::endl;

    uint64_t strobe_1_cnt_addr = STROBE_1_CNT;
    strobe_cnt_reg32 strobe_1_cnt_reg = readRegisterContent(strobe_1_cnt_addr);
    std::cout << "  strobe_1_cnt presence_inq    : " << strobe_1_cnt_reg.presence_inq << std::endl;
    std::cout << "  strobe_1_cnt on_off          : " << strobe_1_cnt_reg.on_off << std::endl;
    std::cout << "  strobe_1_cnt signal_polarity : " << strobe_1_cnt_reg.signal_polarity << std::endl;
    std::cout << "  strobe_1_cnt delay_value     : " << strobe_1_cnt_reg.delay_value << std::endl;
    std::cout << "  strobe_1_cnt duration_value  : " << strobe_1_cnt_reg.duration_value << std::endl;
}

void Camera::captureFrame() {

    cv::Mat camFrame(camFrameHeight, camFrameWidth, CV_8UC1);
    imgIdx = (imgIdx+1) % 8;

    if (testMode) {
        camFrame = testImages[imgIdx];
        /* faking camera image acquisition time */
        eventLoopTimer->setInterval(1000/FRAME_RATE);
    } else {
        dc1394video_frame_t *frame = NULL;
        error = dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_WAIT, &frame);
        camFrame.data = frame->image;
        dc1394_capture_enqueue(camera, frame);
    }
    
    /* cropping image in center to power-of-2 size */
    cv::Rect cropped((camFrame.cols-width)/2, (camFrame.rows-height)/2, width, height);
    camFrame = camFrame(cropped).clone();

    /* assigning image id (current active LED) to pixel in 0,0 */
    camFrame.at<uchar>(0, 0) = imgIdx;
    
    emit newFrame(camFrame);
}

uint32_t Camera::readRegisterContent(uint64_t offset) {

    uint32_t val;
    dc1394_get_control_registers(camera, offset, &val, 1);
    return val;
}

void Camera::writeRegisterContent(uint64_t offset, uint32_t value) {

    dc1394_set_control_registers(camera, offset, &value, 1);
}

void Camera::resetCamera() {

    uint64_t addr = INITIALIZE;
    cam_init_reg32 reg = readRegisterContent(addr);
    reg.init = 1;
    writeRegisterContent(addr, reg);
}

void Camera::configureOutputPins() {

    uint64_t addr = PIO_DIRECTION;
    pio_dir_reg32 reg = readRegisterContent(addr);
    reg.io0_mode = 1;
    reg.io1_mode = 1;
    reg.io2_mode = 0;
    reg.io3_mode = 0;
    writeRegisterContent(addr, reg);
}

void Camera::configureClockDelay() {

    uint64_t addr = STROBE_0_CNT;
    strobe_cnt_reg32 reg = readRegisterContent(addr);
    reg.delay_value = 0;
    writeRegisterContent(addr, reg);
}

void Camera::configureClockDuration() {

    uint64_t addr = STROBE_0_CNT;
    strobe_cnt_reg32 reg = readRegisterContent(addr);
    reg.duration_value = 0x400;
    writeRegisterContent(addr, reg);
}

void Camera::startClockPulse() {

    uint64_t addr = STROBE_0_CNT;
    strobe_cnt_reg32 reg = readRegisterContent(addr);
    reg.duration_value = 0x400;
    reg.delay_value = 0x3E;
    reg.signal_polarity = 1;
    reg.on_off = 1;
    writeRegisterContent(addr, reg);
}

void Camera::stopClockPulse() {

    uint64_t addr = STROBE_0_CNT;
    strobe_cnt_reg32 reg = readRegisterContent(addr);
    reg.on_off = 0;
    writeRegisterContent(addr, reg);
}

void Camera::configureResetDelay() {

    uint64_t addr = STROBE_1_CNT;
    strobe_cnt_reg32 reg = readRegisterContent(addr);
    reg.delay_value = 0;
    writeRegisterContent(addr, reg);
}

void Camera::configureResetDuration() {

    uint64_t addr = STROBE_1_CNT;
    strobe_cnt_reg32 reg = readRegisterContent(addr);
    reg.duration_value = 0x400;
    writeRegisterContent(addr, reg);
}

void Camera::startResetPulse() {

    uint64_t addr = STROBE_1_CNT;
    strobe_cnt_reg32 reg = readRegisterContent(addr);
    reg.duration_value = 0x400;
    reg.on_off = 1;
    reg.signal_polarity = 1;
    writeRegisterContent(addr, reg);
}

void Camera::stopResetPulse() {

    uint64_t addr = STROBE_1_CNT;
    strobe_cnt_reg32 reg = readRegisterContent(addr);
    reg.on_off = 0;
    writeRegisterContent(addr, reg);
}

void Camera::reset() {

    /* reset camera registers and start led ringlight */
    stopClockPulse();
    startResetPulse();
    msleep(100);
    stopResetPulse();
    startClockPulse();
}

void Camera::cleanup(dc1394camera_t *camera) {

    dc1394_video_set_transmission(camera, DC1394_OFF);
    dc1394_capture_stop(camera);
    dc1394_camera_free(camera);
}

void Camera::msleep(unsigned long msecs) {

    QTime sleepTime = QTime::currentTime().addMSecs(msecs);
    while (QTime::currentTime() < sleepTime) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}
