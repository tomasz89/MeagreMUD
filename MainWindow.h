#ifndef __MAINWINDOW
#define __MAINWINDOW

#include <QtCore/QtGlobal>

#include <QMainWindow>

class QMenuBar;
class QMenu;
class QAction;

namespace MeagreMUD
{
    // Forward declarations
    class Console;

    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow();

    public Q_SLOTS:
        void openSettings();

    private:
        void buildMenuBar();

        Console *console;
        QMenuBar *menuBar;
        QMenu *fileMenu;
        QMenu *toolsMenu;
        QAction *exitAction;
        QAction *settingsAction;
    };
}

#endif
