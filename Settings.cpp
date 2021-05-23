#include "Settings.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

QT_USE_NAMESPACE

MeagreMUD::Settings::Settings(QObject *parent) :
    QObject(parent),
    hostName("www.greatermud.com"),
    hostPort(23)
{
}

MeagreMUD::Settings::~Settings()
{
}

void MeagreMUD::Settings::setHost(const QString &host)
{
    hostName = host;
}

void MeagreMUD::Settings::setPort(const quint16 port)
{
    hostPort = port;
}

const QString &MeagreMUD::Settings::host() const
{
    return hostName;
}

quint16 MeagreMUD::Settings::port() const
{
    return hostPort;
}
