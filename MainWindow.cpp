#include "MainWindow.h"
#include "Console.h"
#include "SettingsDialog.h"
#include "Settings.h"

#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>

MeagreMUD::MainWindow::MainWindow(const QString &settingsFile, QWidget *parent) :
    QMainWindow(parent),
    console(new MeagreMUD::Console),
    menuBar(new QMenuBar),
    settings(new MeagreMUD::Settings(this))
{
    console->setEnabled(false);
    buildMenuBar();
    setMenuBar(menuBar);

    setCentralWidget(console);
}

void MeagreMUD::MainWindow::buildMenuBar()
{
    QMenu *fileMenu = new QMenu(tr("&File"), this);

    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    connect(saveAction, SIGNAL(triggered()), this, SLOT(onSaveAction()));
    QAction *saveAsAction = fileMenu->addAction(tr("Save &As"));
    connect(saveAsAction, SIGNAL(triggered()), this, SLOT(onSaveAsAction()));

    fileMenu->addSeparator();
    QAction *connectAction = fileMenu->addAction(tr("&Connect"));
    connect(connectAction, SIGNAL(triggered()), this, SLOT(onConnectAction()));
    fileMenu->addAction(connectAction);

    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("E&xit"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    menuBar->addMenu(fileMenu);

    QMenu *toolsMenu = new QMenu(tr("&Tools"), this);
    QAction *settingsAction = toolsMenu->addAction(tr("&Settings"));
    menuBar->addMenu(toolsMenu);
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(onSettingsAction()));
}

MeagreMUD::MainWindow::~MainWindow()
{
    delete console;
    delete menuBar;
}

void MeagreMUD::MainWindow::onSettingsAction()
{
    MeagreMUD::SettingsDialog *settingsDialog = new SettingsDialog(settings);
    settingsDialog->setWindowModality(Qt::NonModal);
    settingsDialog->show();
}

void MeagreMUD::MainWindow::onSaveAction()
{
    // do nothing.
}

void MeagreMUD::MainWindow::onSaveAsAction()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::AnyFile);
    if (dialog.exec())
    {
        QStringList fileNames = dialog.selectedFiles();

        qDebug () << fileNames;
    }
}

void MeagreMUD::MainWindow::onConnectAction()
{
}
