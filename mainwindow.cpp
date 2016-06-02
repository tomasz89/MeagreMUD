#include <mainwindow.h>

MeagreMud::MeagreMud::MeagreMud()
{
    statusBar()->showMessage("Loading...");
    createMenus();
    createTools();
    statusBar()->showMessage("Ready", 2000);
}

MeagreMud::MeagreMud::~MeagreMud()
{
}

void MeagreMud::MeagreMud::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction("&Save", this, SLOT(save()));
}

void MeagreMud::MeagreMud::createTools()
{
    QToolBar *fileToolBar = addToolBar(tr("Save"));
    fileToolBar->addAction("&Save", this, SLOT(save()));
}

void MeagreMud::MeagreMud::save()
{
}
