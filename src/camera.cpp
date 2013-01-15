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
    
    /* undistort camera matrices */
    K = cv::Mat::eye(3, 3, CV_64FC1);
    K.at<double>(0,0) = 751.79662626559286; /* fx */
    K.at<double>(1,1) = 750.65231364204442; /* fy */
    K.at<double>(0,2) = 306.70009070155015; /* cx */
    K.at<double>(1,2) = 216.11302664191402; /* cy */
    
    dist = cv::Mat::zeros(1, 4, CV_64FC1);
    dist.at<double>(0,0) = -0.38694196102815248;  /* dist1 */
    dist.at<double>(0,1) = 0.15311947060864667;   /* dist2 */
    dist.at<double>(0,2) = 0.002582758567387712;  /* dist3 */
    dist.at<double>(0,3) = 0.0036405418524754108; /* dist4 */
    
    /* minimum resolution of camera */
    camFrameHeight = IMG_HEIGHT;
    camFrameWidth = IMG_WIDTH;
    
    /* we crop images to quadratic dimensions for evenly divisable work-group items */
    height = width = camFrameHeight;

    /* image index iterating modulo 8 */
    imgIdx = START_LED;
    
    /* loading images for test mode */
    std::stringstream s;
    s << PATH_ASSETS << "hand/image_ambient.png";
    ambientImage = cv::imread(s.str(), CV_LOAD_IMAGE_GRAYSCALE);
    avgImgIntensity = cv::mean(ambientImage)[0];
    for (int i=0; i<8; i++) {
        std::stringstream s;
        s << PATH_ASSETS << "hand/image" << i << ".png";
        cv::Mat img = cv::imread(s.str(), CV_LOAD_IMAGE_GRAYSCALE);
        cv::GaussianBlur(img, img, cv::Size(3,3), 1.2);
        testImages.push_back(img);
    }

    /* counter iterating over (modulo 8) LEDs assigning image to current LED */
    imgIdx = 3;
    testMode = false;
    
    /* setup undistortion */
    initUndistLUT();
}

Camera::~Camera() {

    stop();
    delete[] map1LUT;
	delete[] map2LUT;
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
    resetCameraRegister();

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
    
    /* capture image with no LEDs to subtract ambient light */
    captureAmbientImage();
    
    /* set average image intensity used by ps process for adjustment */
    avgImgIntensity = cv::mean(ambientImage)[0];

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

int Camera::avgImageIntensity() {
    
    return avgImgIntensity;
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

void Camera::initUndistLUT() {
    
    int stripeSize = std::min(std::max(1, (1 << 12) / std::max(camFrameWidth, 1)), camFrameHeight);
    cv::Mat map1 = cv::Mat(stripeSize, camFrameWidth, CV_16SC2);
    cv::Mat map2 = cv::Mat(stripeSize, camFrameWidth, CV_16UC1);
    
    map1LUT = new cv::Mat[camFrameHeight];
    map2LUT = new cv::Mat[camFrameHeight];
    
    cv::Mat Ar;
    K.convertTo(Ar, CV_64F);
    
    double v0 = K.at<double>(1,2);
    for (int y = 0; y < camFrameHeight; y += stripeSize) {
        int stripe = std::min(stripeSize, camFrameHeight - y);
        Ar.at<double>(1,2) = v0 - y;
        cv::Mat map1Part = map1.rowRange(0, stripe);
        cv::Mat map2Part = map2.rowRange(0, stripe);
        cv::initUndistortRectifyMap(K, dist, cv::noArray(), Ar, cv::Size(camFrameWidth, stripe), map1Part.type(), map1Part, map2Part);
        map1LUT[y] = map1Part.clone();
        map2LUT[y] = map2Part.clone();
    }
}

void Camera::undistortLUT(cv::InputArray source, cv::OutputArray dest) {
    
    cv::Mat src = source.getMat();
    dest.create(src.size(), src.type());
    cv::Mat dst = dest.getMat();
    
    int stripeSize = std::min(std::max(1, (1 << 12) / std::max(camFrameWidth, 1)), camFrameHeight);
    
    for (int y = 0; y < src.rows; y += stripeSize) {
        int stripe = std::min(stripeSize, src.rows - y);
        cv::Mat map1Part = map1LUT[y];
        cv::Mat map2Part = map2LUT[y];
        cv::Mat destPart = dst.rowRange(y, y + stripe);
        cv::remap(src, destPart, map1Part, map2Part, cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    }
}

void Camera::captureAmbientImage() {
    
    /* capture image with no LEDs to subtract ambient light */
    ambientImage = cv::Mat(camFrameHeight, camFrameWidth, CV_8UC1);
    dc1394video_frame_t *frame = NULL;
    error = dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_WAIT, &frame);
    memcpy(ambientImage.data, frame->image, camFrameHeight*camFrameWidth*sizeof(uchar));
    dc1394_capture_enqueue(camera, frame);
}

void Camera::captureFrame() {

    cv::Mat distortedFrame(camFrameHeight, camFrameWidth, CV_8UC1);
    imgIdx = (imgIdx+1) % 8;

    if (testMode) {
        distortedFrame = testImages[imgIdx].clone();
        /* faking camera image acquisition time */
        eventLoopTimer->setInterval(1000/FRAME_RATE);
    } else {
        dc1394video_frame_t *frame = NULL;
        error = dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_WAIT, &frame);
        distortedFrame.data = frame->image;
        dc1394_capture_enqueue(camera, frame);
    }
    
    /* undistort camera image */
    cv::Mat camFrame(camFrameHeight, camFrameWidth, CV_8UC1);
    undistortLUT(distortedFrame, camFrame);
    
    /* display original frame with ambient light in camera widget */
    cv::Rect cropped((camFrame.cols-width)/2, (camFrame.rows-height)/2, width, height);
    emit newCamFrame(camFrame(cropped).clone());
    
    /* remove ambient light */
    camFrame -= ambientImage;
    
    /* cropping image in center to power-of-2 size */
    cv::Mat croppedFrame = camFrame(cropped).clone();

    /* assigning image id (current active LED) to pixel in 0,0 */
    croppedFrame.at<uchar>(0, 0) = imgIdx;
    
    emit newCroppedFrame(croppedFrame);
}

uint32_t Camera::readRegisterContent(uint64_t offset) {

    uint32_t val;
    dc1394_get_control_registers(camera, offset, &val, 1);
    return val;
}

void Camera::writeRegisterContent(uint64_t offset, uint32_t value) {

    dc1394_set_control_registers(camera, offset, &value, 1);
}

void Camera::resetCameraRegister() {

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
