#include <QApplication>
#include <iostream>
#include "calibration.h"
#include "utils.h"
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    QStringList args = app.arguments();
    
    /* parsing command line arguments */
    if (args.contains("-c") || args.contains("--calibrate")) {
        Calibration::withFourPlanes();
        return 0;
    } else if (args.contains("-d") || args.contains("--debug")) {
        return Utils::diplayLightDirections();
    } else if (args.contains("-h") || args.contains("--help")) {
        std::cout << "Usage: " << args.at(0).toStdString() << " [OPTIONS]" << std::endl;
        std::cout << "\t-c, --calibrate\tcalibrating with four planes" << std::endl;
        std::cout << "\t-d, --debug\tdisplaying light source directions" << std::endl;
        return 0;
    } 
    
    MainWindow mainWin;
    mainWin.show();
    return app.exec();
}

