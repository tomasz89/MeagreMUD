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
    class TerminalWidget;
    class SettingsDialog;
    class Settings;

    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(const QString &settingsFile, QWidget *parent = nullptr);
        ~MainWindow();

    public Q_SLOTS:
        void onSettingsAction();
        void onSaveAction();
        void onSaveAsAction();

    private:
        void buildMenuBar();

        TerminalWidget *terminalWidget;
        QMenuBar *menuBar;
        Settings *settings;
    };
}

#endif
