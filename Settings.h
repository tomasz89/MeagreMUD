#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>
#include <QtGlobal>


QT_USE_NAMESPACE

QT_BEGIN_NAMESPACE
// Qt forward declaractions in here
QT_END_NAMESPACE

namespace MeagreMUD
{
class Settings : public QObject {

    Q_OBJECT

public:
    Settings(QObject *parent = nullptr);
    ~Settings();
    void setHost(const QString &host);
    void setPort(const quint16 port);
    const QString &host() const;
    quint16 port() const;

private:
    QString hostName;
    quint16 hostPort;
};
}
#endif // SETTINGS_H
