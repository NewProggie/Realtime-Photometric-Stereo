#ifndef NORMALSWIDGET_H
#define NORMALSWIDGET_H

#include <vtkSmartPointer.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkPolyLine.h>
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkImageViewer.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkPoints.h>
#include <vtkPerspectiveTransform.h>
#include <vtkMatrixToHomogeneousTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkCamera.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleImage.h>
#include <vtkImageImport.h>
#include <vtkRenderer.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <QVTKWidget.h>

class NormalsWidget : public QVTKWidget {
        
public:
    NormalsWidget(QWidget *parent = 0, int width=0, int height=0);
    ~NormalsWidget();
    void setNormalsImage(cv::Mat img);
    
private:
    /** vtk render window */
    vtkSmartPointer<vtkRenderWindow> renderWindow;
    /** vtk renderer */
    vtkSmartPointer<vtkRenderer> renderer;
    /** vtk image actor for displaying the current frame */
    vtkSmartPointer<vtkImageActor> imgActor;
    /** vtk image data store for the current frame */
    vtkSmartPointer<vtkImageData> imgData;
    /** vtk image importer for converting a CvMat to vtk image data */
    vtkSmartPointer<vtkImageImport> importer;
    /** preparing rendering layer for displaying image */
    void setupRenderLayer(int width, int height);
};

#endif