#ifndef MODELWIDGET_H
#define MODELWIDGET_H

#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkImageViewer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleImage.h>
#include <vtkLight.h>
#include <vtkLightCollection.h>
#include <vtkRenderer.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>
#include <vtkTriangle.h>
#include <vtkOBJExporter.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <QVTKWidget.h>
#include <QtGui/QFileDialog>

class ModelWidget : public QVTKWidget {
    
    Q_OBJECT
    
public:
    ModelWidget(QWidget *parent = 0, int modelWidth = 0, int modelHeight = 0);
    ~ModelWidget();
    /** renders given opencv matrix with xyz-coords and normals structured as a tensor */
    void renderModel(std::vector<cv::Mat> MatXYZN);
    
public slots:
    void exportModel();
    
private:
    vtkSmartPointer<vtkPolyDataMapper> modelMapper;
    vtkSmartPointer<vtkActor> modelActor;
    vtkSmartPointer<vtkLight> light1, light2;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkRenderWindow> renderWindow;
    vtkSmartPointer<vtkPolyData> polyData;
    vtkSmartPointer<vtkCellArray> vtkTriangles;
    vtkSmartPointer<vtkPoints> points;
    
    int modelWidth, modelHeight;
    float *cnp, *cmp;
};

#endif