#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMetaType>
#include <QThread>
#include <QtGui/QMainWindow>
#include <QtGui/QWidget>
#include <QtGui/QMenu>
#include <QtGui/QGroupBox>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QStatusBar>
#include <QtGui/QRadioButton>
#include <QtGui/QPushButton>
#include <QtGui/QCheckBox>
#include <QtGui/QSlider>
#include <QtGui/QDoubleSpinBox>
#include <QtCore/QTimer>
#include <QtCore/QString>
#include <QLabel>
#include <QSizePolicy>
#include <QVTKWidget.h>

#include "camera.h"
#include "camerawidget.h"
#include "modelwidget.h"
#include "normalswidget.h"
#include "photometricstereo.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
public slots:
    void setStatusMessage(QString msg);
    void onModelFinished(std::vector<cv::Mat> MatXYZN);
        
private slots:
    void onTestModeChecked(int state);
    void onViewRadioButtonsChecked(bool checked);
    void onToggleSettingsMenu();
    
private:
    void createInterface();
    
    QWidget *centralWidget;
    QGridLayout *gridLayout, *radioButtonsLayout, *paramsLayout;
    QHBoxLayout *integMethodRadBtnsLayout;
    QLabel *maxpqLabel, *lambdaLabel, *muLabel, *spreadNormsLabel, *unsharpNormsLabel;
    QDoubleSpinBox *maxpqSpinBox, *lambdaSpinBox, *muSpinBox;
    QSlider *spreadNormSlider, *unsharpNormSlider;
    QGroupBox *paramsGroupBox, *integMethodGroupBox;
    QPushButton *exportButton, *toggleSettingsButton;
    QRadioButton *normalsRadioButton, *surfaceRadioButton, *frankChellapRadioButton, *weiKletteRadioButton;
    QCheckBox *testModeCheckBox;
    QThread *camThread;
    
    Camera *camera;
    CameraWidget *camWidget;
    ModelWidget *modelWidget;
    NormalsWidget *normalsWidget;
    PhotometricStereo *ps;
};

#endif