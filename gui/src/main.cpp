#include <QApplication>
#include "window/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MeagreMUD"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("MeagreMUD"));

    MainWindow window;
    window.show();

    return app.exec();
}
