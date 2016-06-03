#ifndef _MAINWINDOW_H
#define _MAINWINDOW_H

#include <QtWidgets>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
QT_END_NAMESPACE

namespace MeagreMud
{

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();
public slots:
    void save();
    void openSettings();
private:
    void createActions();
    void createMenus();
    void createTools();

    QMenu *fileMenu;
    QMenu *optionsMenu;
    QAction *settingsAct;
};

}

#endif
