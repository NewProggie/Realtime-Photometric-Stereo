#include "photometricstereo.h"

PhotometricStereo::PhotometricStereo(int width, int height) : width(width), height(height) {
    
    /* setup pre calibrated global light sources */
    cv::Mat lightSrcs = (cv::Mat_<float>(8,3) <<    -0.222222222222222222,  0.00740740740740740, 0.974967904223579,
                                                    -0.162962962962962980, -0.14074074074074075, 0.9765424294919701,
                                                     0.037037037037037037, -0.20000000000000000, 0.9790956326567478,
                                                     0.148148148148148148, -0.14074074074074075, 0.9788994688404024,
                                                     0.222222222222222222,  0.02962962962962963, 0.9745457244268368,
                                                     0.133333333333333333,  0.14814814814814814, 0.9799358899553055,
                                                    -0.022222222222222222,  0.20000000000000000, 0.9795438595792973,
                                                    -0.155555555555555555,  0.14814814814814814, 0.9766547984503413);
    
    cv::invert(lightSrcs, lightSrcsInv, cv::DECOMP_SVD);

    /* initialize non-changing x,y coords of 3d model */
    XCoords = cv::Mat(height, width, CV_32F, cv::Scalar::all(0));
    YCoords = cv::Mat(height, width, CV_32F, cv::Scalar::all(0));
    for (int y=0; y<width; y++) {
        for (int x=0; x<height; x++) {
            XCoords.at<float>(x, y) = x;
            YCoords.at<float>(x, y) = y;
        }
    }
    
    /* adjustable ps parameters */
    maxpq = 4.0f;
    lambda = 0.5f;
    mu = 0.5f;
    slope = 1.0f;
    unsharpScaleFactor = 1.0f;

    /* counter indicating current active LED */
    imgIdx = START_LED;

    /* vector for storing (8) ps images, initially black */
    for (int i=0; i<8; i++) {
        cv::Mat tmp(height, width, CV_8UC1);
        psImages.push_back(tmp);
    }

    /* initialize OpenCL object and context */
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    cl_context_properties props[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0])(), 0};
    /* try using CPU, since data is large, computations simple and BUS data transfer is slow */
    cl_int clError;
    context = cl::Context(CL_DEVICE_TYPE_CPU, props, NULL, NULL, &clError);
    if (clError != CL_SUCCESS) {
        /* fallback to gpu device */
        context = cl::Context(CL_DEVICE_TYPE_GPU, props, NULL, NULL, &clError);
    }
    devices = context.getInfo<CL_CONTEXT_DEVICES>();

    /* create command queue for OpenCL, using first device available */
    queue = cl::CommandQueue(context, devices[0], 0, &error);

    /* load kernel source */
    int pl;
    std::stringstream s;
    s << PATH_KERNELS << "ps.cl";
    std::string kernelSource = s.str();
    char *programCode = OCLUtils::fileContents(kernelSource.data(), &pl);
    cl::Program::Sources source(1, std::make_pair(programCode, pl));
    program = cl::Program(context, source);

    /* build program */
    program.build(devices);
    std::cout << "Build Status: " << program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(devices[0]) << std::endl;
    std::cout << "Build Log:\t " << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0]) << std::endl;

    /* initialize kernels from program */
    calcNormKernel = cl::Kernel(program, "calcNormals", &error);
    integKernel = cl::Kernel(program, "integrate", &error);
    updateNormKernel = cl::Kernel(program, "updateNormals", &error);

}

PhotometricStereo::~PhotometricStereo() { }

long PhotometricStereo::getMilliSecs() {
    timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000 + t.tv_usec/1000;
}

void PhotometricStereo::setMaxPQ(double val) {
    maxpq = (float) val;
}

float PhotometricStereo::getMaxPQ() {
    return maxpq;
}

void PhotometricStereo::setLambda(double val) {
    lambda = (float) val;
}

float PhotometricStereo::getLambda() {
    return lambda;
}

void PhotometricStereo::setMu(double val) {
    mu = (float) val;
}

float PhotometricStereo::getMu() {
    return mu;
}

void PhotometricStereo::setSlope(int val) {
    slope = (float)(val/100.0f)+1.0f;
}

float PhotometricStereo::getSlope() {
    return slope;
}

void PhotometricStereo::setUnsharpScale(int val) {
    unsharpScaleFactor = (float) (val/100.0f);
}

float PhotometricStereo::getUnsharpScale() {
    return unsharpScaleFactor;
}

cv::Mat PhotometricStereo::readCalibratedLights() {
    
    cv::Mat lightsInv = cv::Mat(height, width, CV_32FC(24), cv::Scalar::all(0));
    
    std::stringstream lmp;
    lmp << PATH_ASSETS << "lightMat.kaw";
    
    FILE *kawFile = fopen(lmp.str().c_str(), "rb");
    if (kawFile == NULL) {
        std::cerr << "ERROR: Could not open calibrated light matrix." << std::endl;
        return lightsInv;
    }
    
    /* get file size */
    long fSize;
    size_t res;
    fseek(kawFile, 0, SEEK_END);
    fSize = ftell(kawFile);
    rewind(kawFile);
    
    /* reading data */
    res = fread(lightsInv.data, 1, sizeof(float)*height*width*lightsInv.channels(), kawFile);
    if (res != fSize) {
        std::cerr << "ERROR: Error while reading calibrated light matrix in" << std::endl;
    }
    fclose(kawFile);
    
    return lightsInv;
}

void PhotometricStereo::setImage(cv::Mat image) {

    /* active led is saved in image at pixel position 0,0 */
    int currIdx = image.at<uchar>(0, 0);

    /* if we previously got a corrupt image we reset our counter at image 0 */
    if (imgIdx == -1 && currIdx == 0) {
        imgIdx = 7;
    }

    /* expecting n+1-th image */
    if ((imgIdx+1)%8 == currIdx) {
        imgIdx = currIdx;
    } else {
        /* corrupt image, waiting for the next sequence */
        imgIdx = -1;
        return;
    }

    /* locking is needed here because psImages is shared between gui- and ps thread */
    mutex.lock();
    image.copyTo(psImages[imgIdx]);
    mutex.unlock();

    /* process photometric stereo every eight images */
    if (imgIdx != -1 && currIdx == 0) {
        /* non-blocking async execution of execute() */
        future = QtConcurrent::run(this, &PhotometricStereo::execute);
    }
}

void PhotometricStereo::execute() {

    /* measuring ps performance */
    long start = getMilliSecs();

    /* creating OpenCL buffers */
    size_t imgSize3 = sizeof(float) * (height*width*3);
    size_t gradSize = sizeof(float) * (height*width);
    size_t sSize = sizeof(float) * (lightSrcsInv.rows*lightSrcsInv.cols*lightSrcsInv.channels());

    cl::ImageFormat imgFormat = cl::ImageFormat(CL_INTENSITY, CL_UNORM_INT8);

    cl_img1 = cl::Image2D(context, CL_MEM_READ_ONLY, imgFormat, width, height, 0, NULL, &error);
    cl_img2 = cl::Image2D(context, CL_MEM_READ_ONLY, imgFormat, width, height, 0, NULL, &error);
    cl_img3 = cl::Image2D(context, CL_MEM_READ_ONLY, imgFormat, width, height, 0, NULL, &error);
    cl_img4 = cl::Image2D(context, CL_MEM_READ_ONLY, imgFormat, width, height, 0, NULL, &error);
    cl_img5 = cl::Image2D(context, CL_MEM_READ_ONLY, imgFormat, width, height, 0, NULL, &error);
    cl_img6 = cl::Image2D(context, CL_MEM_READ_ONLY, imgFormat, width, height, 0, NULL, &error);
    cl_img7 = cl::Image2D(context, CL_MEM_READ_ONLY, imgFormat, width, height, 0, NULL, &error);
    cl_img8 = cl::Image2D(context, CL_MEM_READ_ONLY, imgFormat, width, height, 0, NULL, &error);
    cl_Sinv = cl::Buffer(context, CL_MEM_READ_ONLY, sSize, NULL, &error);
    cl_Pgrads = cl::Buffer(context, CL_MEM_WRITE_ONLY, gradSize, NULL, &error);
    cl_Qgrads = cl::Buffer(context, CL_MEM_WRITE_ONLY, gradSize, NULL, &error);
    cl_N = cl::Buffer(context, CL_MEM_WRITE_ONLY, imgSize3, NULL, &error);

    /* pushing data to CPU */
    cv::Mat Normals(height, width, CV_32FC3, cv::Scalar::all(0));
    cv::Mat Pgrads(height, width, CV_32F, cv::Scalar::all(0));
    cv::Mat Qgrads(height, width, CV_32F, cv::Scalar::all(0));

    cl::size_t<3> origin; origin[0] = 0; origin[1] = 0; origin[2] = 0;
    cl::size_t<3> region; region[0] = width; region[1] = height; region[2] = 1;

    mutex.lock();
    queue.enqueueWriteImage(cl_img1, CL_TRUE, origin, region, 0, 0, psImages.at(0).data);
    queue.enqueueWriteImage(cl_img2, CL_TRUE, origin, region, 0, 0, psImages.at(1).data);
    queue.enqueueWriteImage(cl_img3, CL_TRUE, origin, region, 0, 0, psImages.at(2).data);
    queue.enqueueWriteImage(cl_img4, CL_TRUE, origin, region, 0, 0, psImages.at(3).data);
    queue.enqueueWriteImage(cl_img5, CL_TRUE, origin, region, 0, 0, psImages.at(4).data);
    queue.enqueueWriteImage(cl_img6, CL_TRUE, origin, region, 0, 0, psImages.at(5).data);
    queue.enqueueWriteImage(cl_img7, CL_TRUE, origin, region, 0, 0, psImages.at(6).data);
    queue.enqueueWriteImage(cl_img8, CL_TRUE, origin, region, 0, 0, psImages.at(7).data);
    mutex.unlock();
    queue.enqueueWriteBuffer(cl_Sinv, CL_TRUE, 0, sSize, lightSrcsInv.data, NULL, &event);
    queue.enqueueWriteBuffer(cl_Pgrads, CL_TRUE, 0, gradSize, Pgrads.data, NULL, &event);
    queue.enqueueWriteBuffer(cl_Qgrads, CL_TRUE, 0, gradSize, Qgrads.data, NULL, &event);
    queue.enqueueWriteBuffer(cl_N, CL_TRUE, 0, imgSize3, Normals.data, NULL, &event);

    /* set kernel arguments */
    calcNormKernel.setArg(0, cl_img1); // 1-8 images
    calcNormKernel.setArg(1, cl_img2);
    calcNormKernel.setArg(2, cl_img3);
    calcNormKernel.setArg(3, cl_img4);
    calcNormKernel.setArg(4, cl_img5);
    calcNormKernel.setArg(5, cl_img6);
    calcNormKernel.setArg(6, cl_img7);
    calcNormKernel.setArg(7, cl_img8);
    calcNormKernel.setArg(8, width); // required for..
    calcNormKernel.setArg(9, height); // ..determining array dimensions
    calcNormKernel.setArg(10, cl_Sinv); // inverse of light matrix
    calcNormKernel.setArg(11, cl_Pgrads); // P gradients
    calcNormKernel.setArg(12, cl_Qgrads); // Q gradients
    calcNormKernel.setArg(13, cl_N); // normals for each point
    calcNormKernel.setArg(14, maxpq); // max depth gradients as in [Wei2001]
    calcNormKernel.setArg(15, slope); // exaggerate slope as in [Malzbender2006]

    /* wait for command queue to finish before continuing */
    queue.finish();

    /* executing kernel */
    queue.enqueueNDRangeKernel(calcNormKernel, cl::NullRange, cl::NDRange(height, width), cl::NullRange, NULL, &event);
    queue.finish();

    /* reading back from CPU device */
    queue.enqueueReadBuffer(cl_Pgrads, CL_TRUE, 0, gradSize, Pgrads.data);
    queue.enqueueReadBuffer(cl_Qgrads, CL_TRUE, 0, gradSize, Qgrads.data);
    queue.enqueueReadBuffer(cl_N, CL_TRUE, 0, sizeof(float) * (height*width*3), Normals.data);

    /* integrate and get heights globally */
    cv::Mat Zcoords = getGlobalHeights(Pgrads, Qgrads);
    
    /* pushing normals to CPU again */
    cl_N = cl::Buffer(context, CL_MEM_READ_WRITE, imgSize3, NULL, &error);
    queue.enqueueWriteBuffer(cl_N, CL_TRUE, 0, imgSize3, Normals.data);
    
    /*  unsharp masking as in [Malzbender2006] */
    updateNormKernel.setArg(0, cl_N);
    updateNormKernel.setArg(1, width);
    updateNormKernel.setArg(2, height);
    updateNormKernel.setArg(3, cl_Pgrads);
    updateNormKernel.setArg(4, cl_Qgrads);
    updateNormKernel.setArg(5, unsharpScaleFactor);
    
    /* executing kernel updating normals */
    queue.enqueueNDRangeKernel(updateNormKernel, cl::NullRange, cl::NDRange(height, width), cl::NullRange, NULL, &event);
    queue.finish();
    
    /* reading back from CPU device */
    queue.enqueueReadBuffer(cl_Pgrads, CL_TRUE, 0, gradSize, Pgrads.data);
    queue.enqueueReadBuffer(cl_Qgrads, CL_TRUE, 0, gradSize, Qgrads.data);
    queue.enqueueReadBuffer(cl_N, CL_TRUE, 0, imgSize3, Normals.data);
    
    /* integrate updated gradients second time */
    Zcoords = getGlobalHeights(Pgrads, Qgrads);

    /* store 3d data and normals tensor-like */
    std::vector<cv::Mat> matVec;
    matVec.push_back(XCoords);
    matVec.push_back(YCoords);
    matVec.push_back(Zcoords);
    matVec.push_back(Normals);

    emit executionTime("Elapsed time: " + QString::number(getMilliSecs() - start) + " ms.");
    emit modelFinished(matVec);
}

cv::Mat PhotometricStereo::getGlobalHeights(cv::Mat Pgrads, cv::Mat Qgrads) {

    cv::Mat P(Pgrads.rows, Pgrads.cols, CV_32FC2, cv::Scalar::all(0));
    cv::Mat Q(Pgrads.rows, Pgrads.cols, CV_32FC2, cv::Scalar::all(0));
    cv::Mat Z(Pgrads.rows, Pgrads.cols, CV_32FC2, cv::Scalar::all(0));

    cv::dft(Pgrads, P, cv::DFT_COMPLEX_OUTPUT);
    cv::dft(Qgrads, Q, cv::DFT_COMPLEX_OUTPUT);

    /* creating OpenCL buffers */
    size_t imgSize = sizeof(float) * (height*width*2); /* 2 channel matrix */
    cl_P = cl::Buffer(context, CL_MEM_READ_ONLY, imgSize, NULL, &error);
    cl_Q = cl::Buffer(context, CL_MEM_READ_ONLY, imgSize, NULL, &error);
    cl_Z = cl::Buffer(context, CL_MEM_WRITE_ONLY, imgSize, NULL, &error);

    /* pushing data to CPU */
    queue.enqueueWriteBuffer(cl_P, CL_TRUE, 0, imgSize, P.data, NULL, &event);
    queue.enqueueWriteBuffer(cl_Q, CL_TRUE, 0, imgSize, Q.data, NULL, &event);
    queue.enqueueWriteBuffer(cl_Z, CL_TRUE, 0, imgSize, Z.data, NULL, &event);

    /* set kernel arguments */
    integKernel.setArg(0, cl_P);
    integKernel.setArg(1, cl_Q);
    integKernel.setArg(2, cl_Z);
    integKernel.setArg(3, width);
    integKernel.setArg(4, height);
    integKernel.setArg(5, lambda);
    integKernel.setArg(6, mu);

    /* wait for command queue to finish before continuing */
    queue.finish();

    /* executing kernel */
    queue.enqueueNDRangeKernel(integKernel, cl::NullRange, cl::NDRange(height, width), cl::NullRange, NULL, &event);

    /* reading back from CPU */
    queue.enqueueReadBuffer(cl_Z, CL_TRUE, 0, imgSize, Z.data);

    /* setting unknown average height to zero */
    Z.at<cv::Vec2f>(0, 0)[0] = 0.0f;
    Z.at<cv::Vec2f>(0, 0)[1] = 0.0f;

    cv::dft(Z, Z, cv::DFT_INVERSE | cv::DFT_SCALE |  cv::DFT_REAL_OUTPUT);

    return Z;
}
