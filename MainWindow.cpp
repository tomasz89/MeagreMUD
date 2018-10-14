#include "MainWindow.h"
#include "Console.h"
#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

MeagreMUD::MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    console(new MeagreMUD::Console),
    menuBar(new QMenuBar)
{
    console->setEnabled(false);
    buildMenuBar();
    setMenuBar(menuBar);

    setCentralWidget(console);
}

void MeagreMUD::MainWindow::buildMenuBar()
{
    fileMenu = new QMenu(tr("&File"), this);
    exitAction = fileMenu->addAction(tr("E&xit"));
    menuBar->addMenu(fileMenu);
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    toolsMenu = new QMenu(tr("&Tools"), this);
    settingsAction = toolsMenu->addAction(tr("&Settings"));
    menuBar->addMenu(toolsMenu);
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(openSettings()));
}

MeagreMUD::MainWindow::~MainWindow()
{
    delete console;
    delete menuBar;
}

void MeagreMUD::MainWindow::openSettings()
{
    SettingsDialog *settingsDialog = new SettingsDialog;
    settingsDialog->setWindowModality(Qt::NonModal);
    //connect(settingsDialog, SIGNAL(finished(int)), this, SLOT())
    settingsDialog->show();
}
