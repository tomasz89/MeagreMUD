#ifndef __CONSOLE
#define __CONSOLE

#include <QPlainTextEdit>

QT_BEGIN_NAMESPACE

class QPalette;

QT_END_NAMESPACE

namespace MeagreMUD
{
class Console : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit Console(QWidget *parent = nullptr);
    virtual ~Console();

    void putData(const QByteArray &data);

    void setLocalEchoEnabled(bool set);

signals:
    void getData(const QByteArray &data);

protected:
    virtual void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    virtual void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    virtual void mouseDoubleClickEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    virtual void contextMenuEvent(QContextMenuEvent *e) Q_DECL_OVERRIDE;

private:
    bool localEchoEnabled;
    QPalette *palette;
};
}

#endif
