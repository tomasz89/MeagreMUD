#include <QApplication>
#include "mainwindow.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MeagreMud::MeagreMud meagreMud;
    meagreMud.show();

    return app.exec();
}
