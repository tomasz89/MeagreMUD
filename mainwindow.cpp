#include "mainwindow.h"
#include "configdialog.h"

MeagreMud::MainWindow::MainWindow()
{
    statusBar()->showMessage("Loading...");
    setFocusPolicy(Qt::StrongFocus);
    createActions();
    createMenus();
    createTools();
    statusBar()->showMessage("Ready", 2000);
}

MeagreMud::MainWindow::~MainWindow()
{
}

void MeagreMud::MainWindow::createActions()
{
    //settingsAct = new QAction(QIcon(":/images/new.png"), tr("&New"), this);
    settingsAct = new QAction(tr("&Settings"), this);
    settingsAct->setShortcut(Qt::Key_F4);
    settingsAct->setShortcutContext(Qt::ApplicationShortcut);
    settingsAct->setStatusTip(tr("Open the settings dialog"));
    addAction(settingsAct); // Allows the keyboard shortcut to work.
    connect(settingsAct, SIGNAL(triggered()), this, SLOT(openSettings()));
}

void MeagreMud::MainWindow::createMenus()
{
//    fileMenu = menuBar()->addMenu(tr("&File"));
//    fileMenu->addAction("&Save", this, SLOT(save()));

    optionsMenu = menuBar()->addMenu(tr("&Options"));
    optionsMenu->addAction(settingsAct);
}

void MeagreMud::MainWindow::createTools()
{
//    QToolBar *fileToolBar = addToolBar(tr("Save"));
//    fileToolBar->addAction("&Save", this, SLOT(save()));
}

void MeagreMud::MainWindow::save()
{
}

void MeagreMud::MainWindow::openSettings()
{
    MeagreMud::ConfigDialog dialog;
    dialog.exec();
}
