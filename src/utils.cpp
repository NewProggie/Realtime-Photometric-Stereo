#include "utils.h"

int Utils::diplayLightDirections() {
    
    vtkSmartPointer<vtkStructuredGrid> grid = vtkSmartPointer<vtkStructuredGrid>::New();
    vtkSmartPointer<vtkFloatArray> vectors = vtkSmartPointer<vtkFloatArray>::New();
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkHedgeHog> hedgehog = vtkSmartPointer<vtkHedgeHog>::New();
    vtkSmartPointer<vtkPolyDataMapper> gridMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vtkSmartPointer<vtkActor> gridActor = vtkSmartPointer<vtkActor>::New();
    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    
    /* using quadratic cropped images as provided by camera clas */
    int height = IMG_HEIGHT, width = IMG_HEIGHT;
    int channels = 24;
    /* light directions from file, initially inverted */
    cv::Mat lightsInv = cv::Mat(height, width, CV_32FC(channels), cv::Scalar::all(0));
    /* re-inverted light directions */
    cv::Mat S(height, width, CV_32FC(channels));
    
    /* reading light direction matrix */
    std::stringstream s;
    s << PATH_ASSETS << "lightMat.kaw";
    FILE *file = fopen(s.str().c_str(), "rb");
    
    /* get file size */
    long fSize;
    size_t res;
    fseek(file, 0, SEEK_END);
    fSize = ftell(file);
    rewind(file);
    
    /* reading data */
    res = fread(lightsInv.data, 1, sizeof(float)*height*width*lightsInv.channels(), file);
    if (res != fSize) {
        std::cerr << "ERROR: Error while reading calibrated light matrix" << std::endl;
        return 1;
    }
    fclose(file);
    
    /* create point field with appropriate vectors */
    static int dims[3] = { height/4, width/4, 1};
    grid->SetDimensions(dims);
    vectors->SetNumberOfComponents(3);
    vectors->SetNumberOfTuples(dims[0]*dims[1]*dims[2]);
    points->Allocate(dims[0]*dims[1]*dims[2]);
    
    int idx = 0;
    for (int row=0; row < dims[0]; row++) {
        for (int col=0; col < dims[1]; col++) {
            
            /* local light matrix initially inverted */
            cv::Mat s(3, 8, CV_32F, cv::Scalar::all(0));

            /* filling up matrix from saved file */
            for (int i=0; i<8; i++) {
                /* offset: (row * numCols * numChannels) + (col * numChannels) + (channel) */
                ((float*)s.data)[(0*8*1)+(i*1)+(0)] = ((float*)lightsInv.data)[(row*width*channels)+(col*channels)+(i*3+0)];
                ((float*)s.data)[(1*8*1)+(i*1)+(0)] = ((float*)lightsInv.data)[(row*width*channels)+(col*channels)+(i*3+1)];
                ((float*)s.data)[(2*8*1)+(i*1)+(0)] = ((float*)lightsInv.data)[(row*width*channels)+(col*channels)+(i*3+2)];
            }
            
            /* re-invert local light matrix */
            cv::invert(s, s, cv::DECOMP_SVD);
            /* filling up normal vector field */
            points->InsertPoint(idx++, row, col, 1);
            float* f = new float[3];
            int index = 6;
            f[0] = ((float*) s.data)[(index*3*1)+(0*1)+(0)];
            f[1] = ((float*) s.data)[(index*3*1)+(1*1)+(0)];
            f[2] = ((float*) s.data)[(index*3*1)+(2*1)+(0)];
            vtkMath::Normalize(f);
            vectors->InsertTuple(idx, f);
            delete [] f;
        }
    }
    
    grid->SetPoints(points);
    grid->GetPointData()->SetVectors(vectors);
    
    /* create vtk renderpipeline */
    hedgehog->SetInput(grid);
    hedgehog->SetScaleFactor(5);
    gridMapper->SetInputConnection(hedgehog->GetOutputPort());
    gridActor->SetMapper(gridMapper);
    gridActor->GetProperty()->SetColor(1, 1, 1);
    renderWindow->AddRenderer(renderer);
    renderer->AddActor(gridActor);
    renderer->SetGradientBackground(true);
    renderer->SetBackground(.45, .45, .9);
    renderer->SetBackground2(.0, .0, .0);
    renderer->ResetCamera();
    renderWindow->SetSize(400, 400);
    
    interactor->SetRenderWindow(renderWindow);
    renderWindow->Render();
    interactor->Start();
    
    return 0;
}