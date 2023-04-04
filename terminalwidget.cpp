#include "terminalwidget.h"
#include "Settings.h"

#include <QtWidgets>
#include <QtNetwork>
#include <QColor>

MeagreMUD::TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent),
      m_socket(0),
      m_textEdit(0),
      m_lineEdit(0),
      settings(0)
{
    // Set up the UI
    m_textEdit = new QTextEdit(this);
    m_textEdit->setTextBackgroundColor(Qt::GlobalColor::black);
    m_textEdit->setTextColor(Qt::GlobalColor::white);
    m_textEdit->setLineWrapMode(QTextEdit::LineWrapMode::NoWrap);


    m_lineEdit = new QLineEdit(this);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(m_textEdit);
    layout->addWidget(m_lineEdit);
    setLayout(layout);

    // Set up the socket
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &TerminalWidget::onReadyRead);

    // Set up the line edit
    connect(m_lineEdit, &QLineEdit::returnPressed, [this]() {
        QByteArray data = m_lineEdit->text().toUtf8();
        m_socket->write(data);
        m_textEdit->append(data);
        m_lineEdit->clear();
    });
}

void MeagreMUD::TerminalWidget::onSettingsChanged(MeagreMUD::Settings *newSettings)
{
    settings = newSettings;
}

void MeagreMUD::TerminalWidget::onConnectAction()
{
    qDebug() << "onConnect";
    if (settings)
    {
        qDebug() << "Connecting to " << settings->host() << ":" << settings->port();
        m_socket->connectToHost(settings->host(), settings->port());
    }
}

void MeagreMUD::TerminalWidget::onReadyRead()
{
    qDebug() << "on RR";
    QString data = QString::fromUtf8(m_socket->readAll());
    QStringList lines = data.split("\n"); // what if you do not get a full line..??

    for (const QString &line : lines) {
        qDebug() << "raw line : " << line;
        QString formattedLine;
        QString currentStyle;

        for (int i = 0; i < line.length(); ++i) {
            if (line[i] == '\033') {
                qDebug() << " found esc seq";
                // Parse the escape sequence
                QString escapeSequence;
                for (i++; i < line.length(); ++i) {
                    if (line[i] == 'm') {
                        escapeSequence += line[i];
                        break;
                    } else {
                        escapeSequence += line[i];
                    }
                }

                // Apply the escape sequence to the current style
                QStringList params = escapeSequence.split(";");
                for (const QString &param : params) {
                    bool ok;
                    int value = param.toInt(&ok);
                    if (ok) {
                        switch (value) {
                            case 0: // Reset
                                currentStyle.clear();
                                break;
                            case 1: // Bold
                                currentStyle += "font-weight:bold;";
                                break;
                            case 30: // Foreground color: Black
                                currentStyle += "color:black;";
                                break;
                            case 31: // Foreground color: Red
                                currentStyle += "color:red;";
                                break;
                            case 32: // Foreground color: Green
                                currentStyle += "color:green;";
                                break;
                            case 33: // Foreground color: Yellow
                                currentStyle += "color:yellow;";
                                break;
                            case 34: // Foreground color: Blue
                                currentStyle += "color:blue;";
                                break;
                            case 35: // Foreground color: Magenta
                                currentStyle += "color:magenta;";
                                break;
                            case 36: // Foreground color: Cyan
                                currentStyle += "color:cyan;";
                                break;
                            case 37: // Foreground color: White
                                currentStyle += "color:white;";
                                break;
                            case 40: // Background color: Black
                                currentStyle += "background-color:black;";
                                break;
                            case 41: // Background color: Red
                                currentStyle += "background-color:red;";
                                break;
                            case 42: // Background color: Green
                                currentStyle += "background-color:green;";
                                break;
                            case 43: // Background color: Yellow
                                currentStyle += "background-color:yellow;";
                                break;
                            case 44: // Background color: Blue
                                currentStyle += "background-color:blue;";
                                break;
                            case 45: // Background color: Magenta
                                currentStyle += "background-color:magenta;";
                                break;
                            case 46: // Background color: Cyan
                                currentStyle += "background-color:cyan;";
                                break;
                            case 47: // Background color: White
                                currentStyle += "background-color:white;";
                                break;
                        }
                    }
                }
            } else {
                // Append the character to the formatted line
                QString escapedChar = QString(line[i]).toHtmlEscaped();
                formattedLine += QString("<span style=\"%1\">%2</span>")
                                  .arg(currentStyle)
                                  .arg(escapedChar);
            }
        }

        // Append the formatted line to the text edit
        m_textEdit->append(formattedLine);
}

}
