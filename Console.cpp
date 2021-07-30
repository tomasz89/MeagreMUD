#include "Console.h"

#include <QScrollBar>

#include <QtCore/QDebug>

MeagreMUD::Console::Console(QWidget *parent) :
    QPlainTextEdit(parent),
    localEchoEnabled(false)
{
    document()->setMaximumBlockCount(100);
    QPalette p = palette();
    p.setColor(QPalette::Base, Qt::black);
    p.setColor(QPalette::Text, Qt::green);
    setPalette(p);
}

MeagreMUD::Console::~Console()
{
}

void MeagreMUD::Console::putData(const QByteArray &data)
{
    insertPlainText(QString(data));

    QScrollBar *bar = verticalScrollBar();
    bar->setValue(bar->maximum());
}

void MeagreMUD::Console::setLocalEchoEnabled(bool set)
{
    localEchoEnabled = set;
}

void MeagreMUD::Console::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Backspace:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
        break;
    default:
        if (localEchoEnabled)
            QPlainTextEdit::keyPressEvent(e);
        emit getData(e->text().toLocal8Bit());
    }
}

void MeagreMUD::Console::mousePressEvent(QMouseEvent *e)
{
    Q_UNUSED(e)
    setFocus();
}

void MeagreMUD::Console::mouseDoubleClickEvent(QMouseEvent *e)
{
    Q_UNUSED(e)
}

void MeagreMUD::Console::contextMenuEvent(QContextMenuEvent *e)
{
    Q_UNUSED(e)
}
