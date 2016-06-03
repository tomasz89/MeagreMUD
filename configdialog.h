#ifndef _CONFIGDIALOG_H
#define _CONFIGDIALOG_H

#include <QDialog>

class QListWidget;
class QListWidgetItem;
class QStackedWidget;

namespace MeagreMud
{

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    ConfigDialog();

    QListWidget *contentsWidget;
    QStackedWidget *pagesWidget;
};

}

#endif
