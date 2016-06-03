#include <QApplication>
#include "mainwindow.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MeagreMud::MainWindow meagreMud;
    meagreMud.show();

    return app.exec();
}
