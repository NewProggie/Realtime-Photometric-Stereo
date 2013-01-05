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
    
    /* toggle settings button */
    toggleSettingsButton = new QPushButton("Show settings menu", centralWidget);
    toggleSettingsButton->setCheckable(true);
    connect(toggleSettingsButton, SIGNAL(clicked()), this, SLOT(onToggleSettingsMenu()));
    gridLayout->addWidget(toggleSettingsButton, 2, 0);

    /* export 3d model button */
    exportButton = new QPushButton("Export 3D Model", centralWidget);
    connect(exportButton, SIGNAL(clicked()), modelWidget, SLOT(exportModel()));
    gridLayout->addWidget(exportButton, 2, 1);

    /* add settings to adjust ps parameter and export 3d model */
    paramsGroupBox = new QGroupBox("Adjust parameter");
    paramsLayout = new QGridLayout();
    
    maxpqLabel = new QLabel("max<sub>pq</sub>", paramsGroupBox);
    maxpqSpinBox = new QDoubleSpinBox(paramsGroupBox);
    maxpqSpinBox->setRange(0, 100);
    maxpqSpinBox->setValue(ps->getMaxPQ());
    connect(maxpqSpinBox, SIGNAL(valueChanged(double)), ps, SLOT(setMaxPQ(double)));
    paramsLayout->addWidget(maxpqLabel, 0, 0);
    paramsLayout->addWidget(maxpqSpinBox, 0, 1);
    
    lambdaLabel = new QLabel("<html><body>&lambda;<body></html>", paramsGroupBox);
    lambdaSpinBox = new QDoubleSpinBox(paramsGroupBox);
    lambdaSpinBox->setRange(0, 1);
    lambdaSpinBox->setSingleStep(0.1);
    lambdaSpinBox->setValue(ps->getLambda());
    connect(lambdaSpinBox, SIGNAL(valueChanged(double)), ps, SLOT(setLambda(double)));
    paramsLayout->addWidget(lambdaLabel, 1, 0);
    paramsLayout->addWidget(lambdaSpinBox, 1, 1);
    
    muLabel = new QLabel("<html><body>&mu;<body></html>", paramsGroupBox);
    muSpinBox = new QDoubleSpinBox(paramsGroupBox);
    muSpinBox->setRange(0, 1);
    muSpinBox->setSingleStep(0.1);
    muSpinBox->setValue(ps->getMu());
    connect(muSpinBox, SIGNAL(valueChanged(double)), ps, SLOT(setMu(double)));
    paramsLayout->addWidget(muLabel, 2, 0);
    paramsLayout->addWidget(muSpinBox, 2, 1);
    
    spreadNormsLabel = new QLabel("Spread surface normals", paramsGroupBox);
    spreadNormSlider = new QSlider(Qt::Horizontal, paramsGroupBox);
    spreadNormSlider->setRange(1, 100);
    spreadNormSlider->setValue((int)ps->getSlope());
    connect(spreadNormSlider, SIGNAL(valueChanged(int)), ps, SLOT(setSlope(int)));
    paramsLayout->addWidget(spreadNormsLabel, 3, 0);
    paramsLayout->addWidget(spreadNormSlider, 3, 1);
    
    unsharpNormsLabel = new QLabel("Unsharp masking normals", paramsGroupBox);
    unsharpNormSlider = new QSlider(Qt::Horizontal, paramsGroupBox);
    unsharpNormSlider->setRange(1, 300);
    unsharpNormSlider->setValue((int)ps->getUnsharpScale()*100);
    connect(unsharpNormSlider, SIGNAL(valueChanged(int)), ps, SLOT(setUnsharpScale(int)));
    paramsLayout->addWidget(unsharpNormsLabel, 4, 0);
    paramsLayout->addWidget(unsharpNormSlider, 4, 1);
    
    paramsGroupBox->setLayout(paramsLayout);
    paramsGroupBox->hide();
    gridLayout->addWidget(paramsGroupBox, 3, 0);
    
    setCentralWidget(centralWidget);
}

void MainWindow::onToggleSettingsMenu() {
    
    toggleSettingsButton->setText(QString("%1 settings menu").arg(toggleSettingsButton->isChecked() ? "Hide": "Show"));
    paramsGroupBox->setVisible(toggleSettingsButton->isChecked());
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
