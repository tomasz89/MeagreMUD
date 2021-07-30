#include <QApplication>

#include "MainWindow.h"

int main(int argc, char **argv)
{
    QCoreApplication::setOrganizationName("TheOpenSourceCommunity");
    QCoreApplication::setOrganizationDomain("github.com");
    QCoreApplication::setApplicationName("MeagreMUD");

    QApplication application(argc, argv);

    QString settingsFile;
    if (argc > 0)
    {
        settingsFile = QString(argv[1]);
    }

    MeagreMUD::MainWindow window(settingsFile);
    window.show();
    return application.exec();
}
