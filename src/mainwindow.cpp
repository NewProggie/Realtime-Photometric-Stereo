#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {

    /* register several types in order to use it for qt signals/slots */
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType< std::vector<cv::Mat> >("std::vector<cv::Mat>");

    /* setup camera */
    camera = new Camera();
    bool camFound = camera->open(0);
    if (!camFound) {
        /* no camera, forcing test mode */
        camera->setTestMode(true);
    } else {
        /* reset camera registers and start led ringlight */
        camera->reset();
        camera->printStatus();
    }
    
    camThread = new QThread;
    camera->moveToThread(camThread);
        
    /* creating photometric stereo process */
    ps = new PhotometricStereo(camera->width, camera->height);

    /* setup ui */
    setWindowTitle("Realtime Photometric-Stereo");
    createInterface();
    statusBar()->setStyleSheet("font-size:12px;font-weight:bold;");

    /* connecting camera with attached thread */
    connect(camThread, SIGNAL(started()), camera, SLOT(start()));
    connect(camera, SIGNAL(stopped()), camThread, SLOT(quit()));
    connect(camera, SIGNAL(stopped()), camera, SLOT(deleteLater()));
    connect(camThread, SIGNAL(finished()), camThread, SLOT(deleteLater()));
    
    /* connecting camera with camerawidget and ps process */
    connect(camera, SIGNAL(newFrame(cv::Mat)), camWidget, SLOT(setImage(cv::Mat)), Qt::AutoConnection);
    /* invoking ps setImage slot immediately, when the signal is emitted to ensure image order */
    connect(camera, SIGNAL(newFrame(cv::Mat)), ps, SLOT(setImage(cv::Mat)), Qt::DirectConnection);
    
    /* connecting ps process with mainwindow and modelwidget */
    connect(ps, SIGNAL(executionTime(QString)), this, SLOT(setStatusMessage(QString)), Qt::AutoConnection);
    connect(ps, SIGNAL(modelFinished(std::vector<cv::Mat>)), this, SLOT(onModelFinished(std::vector<cv::Mat>)), Qt::AutoConnection);
    
    /* start camera in separate thread with high priority */
    camThread->start();
    camThread->setPriority(QThread::TimeCriticalPriority);
}

MainWindow::~MainWindow() {

    /* cleaning up */
    if (!camera->inTestMode()) {
        camera->stop();
    }
    camThread->quit();
    
    delete ps;
    delete camThread;
    delete camWidget;
    delete modelWidget;
    delete normalsWidget;
    delete testModeCheckBox;
    delete exportButton;
    delete radioButtonsLayout;
    delete gridLayout;
    delete centralWidget;
}

void MainWindow::createInterface() {

    /** building UI **/
    centralWidget = new QWidget(this);
    gridLayout = new QGridLayout(centralWidget);

    /* camera preview */
    camWidget = new CameraWidget(centralWidget, camera->width, camera->height);
    camWidget->setMinimumSize(320, 240);
    gridLayout->addWidget(camWidget, 0, 0);

    /* surface normals */
    normalsWidget = new NormalsWidget(centralWidget, camera->width, camera->height);
    normalsWidget->setMinimumWidth(300);
    /* initially 3d reconstruction will be displayed */
    normalsWidget->hide();
    gridLayout->addWidget(normalsWidget, 0, 1);

    /* 3D reconstruction */
    modelWidget = new ModelWidget(centralWidget, camera->width, camera->height);
    modelWidget->setMinimumWidth(300);
    gridLayout->addWidget(modelWidget, 0, 1);

    /* switching between displaying reconstruction and surface normals */
    radioButtonsLayout = new QGridLayout();
    normalsRadioButton = new QRadioButton("Surface normals");
    connect(normalsRadioButton, SIGNAL(toggled(bool)), this, SLOT(onViewRadioButtonsChecked(bool)));
    radioButtonsLayout->addWidget(normalsRadioButton, 0, 0);

    surfaceRadioButton = new QRadioButton("3D Reconstruction");
    surfaceRadioButton->setChecked(true);
    connect(surfaceRadioButton, SIGNAL(toggled(bool)), this, SLOT(onViewRadioButtonsChecked(bool)));
    radioButtonsLayout->addWidget(surfaceRadioButton, 0, 1);

    gridLayout->addLayout(radioButtonsLayout, 1, 1);

    /* test modus using 8 prev taken photos */
    testModeCheckBox = new QCheckBox("Test/Presentation mode");
    connect(testModeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(onTestModeChecked(int)));
    if (camera->inTestMode()) {
        /* no camera found, forcing test mode */
        testModeCheckBox->setChecked(true);
        testModeCheckBox->setDisabled(true);
    }
    gridLayout->addWidget(testModeCheckBox, 1, 0);

    /* exporting ps images */
    exportButton = new QPushButton("Export as OBJ", centralWidget);
    connect(exportButton, SIGNAL(clicked()), modelWidget, SLOT(exportModel()));
    gridLayout->addWidget(exportButton, 2, 1);

    setCentralWidget(centralWidget);
}

void MainWindow::onTestModeChecked(int state) {

    if (state > 0) {
        /* checkbox is active */
        camera->setTestMode(true);
    } else {
        camera->setTestMode(false);
    }
}

void MainWindow::setStatusMessage(QString msg) {

    statusBar()->showMessage(msg);
}

void MainWindow::onModelFinished(std::vector<cv::Mat> MatXYZN) {

    /* we either display object normals or complete reconstruction */
    modelWidget->renderModel(MatXYZN);
    cv::Mat Normals = MatXYZN.back();
    normalsWidget->setNormalsImage(Normals);
}

void MainWindow::onViewRadioButtonsChecked(bool checked) {

    if (normalsRadioButton->isChecked() && normalsWidget->isHidden()) {
        modelWidget->hide();
        normalsWidget->show();
    } else if (surfaceRadioButton->isChecked() && modelWidget->isHidden()) {
        normalsWidget->hide();
        modelWidget->show();
    }
}
