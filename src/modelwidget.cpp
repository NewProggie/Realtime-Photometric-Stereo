#include "modelwidget.h"

ModelWidget::ModelWidget(QWidget *parent, int modelWidth, int modelHeight) : QVTKWidget(parent), modelWidth(modelWidth), modelHeight(modelHeight) {

    /* creating visualization pipeline which basically looks like this:
     vtkPoints -> vtkPolyData -> vtkPolyDataMapper -> vtkActor -> vtkRenderer */
    points = vtkSmartPointer<vtkPoints>::New();
    polyData = vtkSmartPointer<vtkPolyData>::New();
    modelMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    modelActor = vtkSmartPointer<vtkActor>::New();
    renderer = vtkSmartPointer<vtkRenderer>::New();
    vtkTriangles = vtkSmartPointer<vtkCellArray>::New();

    /* setup non-changing x,y coords */
    for (int y=0; y<modelHeight; y++) {
        for (int x=0; x<modelWidth; x++) {
            points->InsertNextPoint(x, y, 1.0);
        }
    }
    
    /* reused memory holding normals and 3d data */
    cnp = new float[modelWidth*modelHeight*3];
    cmp = new float[modelWidth*modelHeight*3];

    /* setup the connectivity between grid points */
    vtkSmartPointer<vtkTriangle> triangle = vtkSmartPointer<vtkTriangle>::New();
    triangle->GetPointIds()->SetNumberOfIds(3);
    for (int i=0; i<modelHeight-1; i++) {
        for (int j=0; j<modelWidth-1; j++) {
            triangle->GetPointIds()->SetId(0, j+(i*modelWidth));
            triangle->GetPointIds()->SetId(1, (i+1)*modelWidth+j);
            triangle->GetPointIds()->SetId(2, j+(i*modelWidth)+1);
            vtkTriangles->InsertNextCell(triangle);
            triangle->GetPointIds()->SetId(0, (i+1)*modelWidth+j);
            triangle->GetPointIds()->SetId(1, (i+1)*modelWidth+j+1);
            triangle->GetPointIds()->SetId(2, j+(i*modelWidth)+1);
            vtkTriangles->InsertNextCell(triangle);
        }
    }
    polyData->SetPoints(points);
    polyData->SetPolys(vtkTriangles);
    
    /* create two scene lights illuminating both sides of 2.5D model */
    light1 = vtkSmartPointer<vtkLight>::New();
    light1->SetPosition(0, 0, -1);
    light1->SetLightTypeToSceneLight();
    renderer->AddLight(light1);
    
    light2 = vtkSmartPointer<vtkLight>::New();
    light2->SetPosition(0, 0, 1);
    light2->SetLightTypeToSceneLight();
    renderer->AddLight(light2);

    modelMapper->SetInput(polyData);
    /* immediate render mode faster/better for large datasets */
    modelMapper->ImmediateModeRenderingOn();
    renderer->SetBackground(.45, .45, .9);
    renderer->SetBackground2(.0, .0, .0);
    renderer->GradientBackgroundOn();
    renderWindow = GetRenderWindow();
    renderWindow->AddRenderer(renderer);
    modelActor->SetMapper(modelMapper);
    
    /* setting some properties to make it look just right */
    modelActor->GetProperty()->SetSpecular(0.25);
    modelActor->GetProperty()->SetAmbient(0.25);
    modelActor->GetProperty()->SetDiffuse(0.25);
    modelActor->GetProperty()->SetInterpolationToPhong();
    
    renderer->AddActor(modelActor);
}

ModelWidget::~ModelWidget() {

    delete [] cnp;
    delete [] cmp;
}

void ModelWidget::renderModel(std::vector<cv::Mat> MatXYZN) {
    
    /* splitting normals from x,y,z coords */
    cv::Mat Normals = MatXYZN.back();
    MatXYZN.pop_back();
    float *np = &Normals.at<float>(0);
    /* memcpy(void* destination, void* source, size_t num) */
    memcpy(cnp, np, modelWidth*modelHeight*3*sizeof(float));
    vtkFloatArray *nArray = vtkFloatArray::New();
    nArray->SetNumberOfComponents(3);
    nArray->SetArray(cnp, modelWidth*modelHeight*3, 1);
    
    polyData->GetPointData()->SetNormals(nArray);

    /* pointing to first entry of a 3-channel matrix
     with x,y,z coords according to channel */
    cv::Mat Model;
    cv::merge(MatXYZN, Model);
    float *mp = &Model.at<float>(0);
    memcpy(cmp, mp, modelWidth*modelHeight*3*sizeof(float));
    vtkFloatArray *fArray = vtkFloatArray::New();
    fArray->SetNumberOfComponents(3);
    fArray->SetArray(cmp, modelWidth*modelHeight*3, 1);

    points->Reset();
    points->SetData(fArray);

    /* Modified() is expensive and therefore not called automatically
     if underlying data has changed */
    points->Modified();

    /* refreshing the widget to display the new data */
    update();

    /* cleaning up */
    nArray->Delete();
    fArray->Delete();
}

void ModelWidget::exportModel() {

    QString filename = QFileDialog::getSaveFileName(this, "Export model", "", "Polygon File Format (*.ply);;Wavefront OBJ (*.obj);;Stereolithography (*.stl)");
    
    QFileInfo fi(filename);
    QString ext = fi.suffix();
    
    if (ext.compare("ply") == 0) {
        vtkSmartPointer<vtkPLYWriter> plyExporter = vtkSmartPointer<vtkPLYWriter>::New();
        plyExporter->SetInput(polyData);
        plyExporter->SetFileName(filename.toStdString().c_str());
        plyExporter->SetColorModeToDefault();
        plyExporter->SetArrayName("Colors");
        plyExporter->Update();
        plyExporter->Write();
    } else if (ext.compare("obj") == 0) {
        vtkSmartPointer<vtkOBJExporter> objExporter = vtkSmartPointer<vtkOBJExporter>::New();
        objExporter->SetInput(renderWindow);
        objExporter->SetFilePrefix(filename.toStdString().c_str());
        objExporter->Update();
        objExporter->Write();
    } else {
        vtkSmartPointer<vtkSTLWriter> stlExporter = vtkSmartPointer<vtkSTLWriter>::New();
        stlExporter->SetInput(polyData);
        stlExporter->SetFileName(filename.toStdString().c_str());
        stlExporter->SetFileTypeToBinary();
        stlExporter->Update();
        stlExporter->Write();
    }

}
