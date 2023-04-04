#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QObject>
#include <QWidget>

// Forward declarations
class QTcpSocket;
class QTextEdit;
class QLineEdit;

namespace MeagreMUD {
// Forward declarations
class Settings;

class TerminalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);

signals:

public Q_SLOTS:
    void onSettingsChanged(MeagreMUD::Settings *newSettings);
    void onConnectAction();

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket;
    QTextEdit *m_textEdit;
    QLineEdit *m_lineEdit;
    Settings *settings;
};

}

#endif // TERMINALWIDGET_H
